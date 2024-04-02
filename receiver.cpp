/*
* OSD Picture in Picture plugin for the Video Disk Recorder
*
* Copyright (C) 2010        Mitchm at vdrportal.de
* Copyright (C) 2004 - 2008 Andreas Regel <andreas.regel@powarman.de>
* Copyright (C) 2004        Sascha Volkenandt <sascha@akv-soft.de>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "receiver.h"
//#include "pes.h"
#include "setup.h"
//#include "remux.h"
#include "tools.h"
#include <algorithm>
#include <cstring>

//#include <vdr/channels.h>
#include "ringbuffer.h"

#ifdef _MSC_VER
static void *memmem(const void *_haystack, size_t haystacklen, const void *_needle, size_t needlelen) {
	auto *haystack = (uint8_t *)_haystack;
	auto *needle = (uint8_t *)_needle;
	if (!needlelen)
		return haystack;
	if (haystacklen < needlelen)
		return nullptr;
	uint8_t *end = haystack + haystacklen - needlelen;
	while (haystack = (uint8_t *)memchr(haystack, *needle, end - haystack)) {
		if (memcmp(haystack, needle, needlelen) == 0)
			return haystack;
		else
			++haystack;
	}
	return nullptr;
}
#endif

//////////////////////////////////////////////////////////////////////////
// cTs2Video
//////////////////////////////////////////////////////////////////////////

class cTs2Video {
public:
	cTs2Video() : data(this) {}
	virtual ~cTs2Video() { free(_data); }
	virtual bool PutTs(const uint8_t *Data) = 0;
	virtual uint8_t *Get(int &Count, ePictureType *PictureType) = 0;
	virtual void Del() {
		if (start) {
			memmove(_data, data, length);
			start = 0;
			pictureStart = -1;
		}
	}
protected:
	uint8_t *_data = nullptr;
	int32_t size = 0;
	int32_t start = 0;
	int32_t length = 0;
	int32_t pictureStart = -1;
	// = _data + start
	class cData {
	public:
		cData(cTs2Video *Parent) : parent(Parent) {}
		//cData &operator=(uint8_t *set) { parent->_data = set; return *this; }
		operator uint8_t *() { return parent->_data + parent->start; }
	private:
		cTs2Video *parent;
	} data;
};


//////////////////////////////////////////////////////////////////////////
// cTs2Mpeg2Video
//////////////////////////////////////////////////////////////////////////

class cTs2Mpeg2Video : public cTs2Video {
public:
	virtual bool PutTs(const uint8_t *Data) override;
	virtual uint8_t *Get(int &Count, ePictureType *PictureType) override;
private:
	enum {
		SYNCING,
		FIND_PICTURE,
		SCAN_PICTURE,
	} state = SYNCING;
	int32_t groupStart = -1;
};

bool cTs2Mpeg2Video::PutTs(const uint8_t *Data) {
	int Length;

	if (TsPayloadStart(Data)) {
		Length = TsGetPayload(&Data);
		int PesLength = ::PesLength(Data) - 6;
		if (PesLength)
			__debugbreak();
			int o = PesPayloadOffset(Data);
			Data += o;
			Length -= o;
	} else
		Length = TsGetPayload(&Data);

	if (start + length + Length > size) {
		int newSize = std::max(KILOBYTE(2), (start + length + Length + KILOBYTE(2) - 1) & ~(KILOBYTE(2) - 1));
		if (auto *newData = (uint8_t *)realloc(_data, newSize)) {
			_data = newData;
			size = newSize;
		} else {
			esyslog("ERROR: out of memory");
			//			Reset();
			__debugbreak();
			return false;
		}
	}
	memcpy(data + length, Data, Length);
	length += Length;
	static uint8_t search[] = { 0x00, 0x00, 0x01, 0x00 };

	if (state == SYNCING && length >= 4) {
		uint8_t *found = data;
		uint8_t *end = data + length - 4;
		while ((state == SYNCING) && (found = (uint8_t*)memmem(found, end - found, search, 3))) {
			switch (found[3]) {
			case 0x00: // picture start code
				pictureStart = found - data;
			case 0xB8: // group start code
				if (found[3]) groupStart = found - data;
			case 0xB7: // sequence end code
			case 0xB3: // sequence header code
				state = FIND_PICTURE;
				length = (Length = end - found) + 4;
				memmove(data, found, length);
				break;
			default:
				++found;
				break;
			}
		}
	}

	if (state == FIND_PICTURE && Length) {
		uint8_t *found = data + length - Length - 3;
		uint8_t *end = data + length - 4;
		while ((state == FIND_PICTURE) && (found = (uint8_t*)memmem(found, end - found, search, 3))) {
			if (found[3] >= 0x01 && found[3] <= 0xAF) {
				state = SCAN_PICTURE;
				Length = end - found;
				break;
			} else {
				switch (found[3]) {
				case 0x00: // picture start code
					pictureStart = found - data;
					break;
				case 0xB8: // group start code
					groupStart = found - data;
					break;
				}
				++found;
			}
		}
	}
	if (state == SCAN_PICTURE && Length) {
		uint8_t *found = data + length - Length - 3;
		uint8_t *end = data + length - 4;
		while ((state == SCAN_PICTURE) && (found = (uint8_t*)memmem(found, end - found, search, 3))) {
			switch (found[3]) {
			case 0x00: // picture start code
//				pictureStart = found - data;
			case 0xB8: // group start code
//				if (found[3])groupStart = found - data;
			case 0xB7: // sequence end code
			case 0xB3: // sequence header code
				state = SYNCING;
				start = found - _data;
				length = end - found + 4;
				break;
			default:
				++found;
				break;
			}
		}
	}
	return start != 0;
}

uint8_t * cTs2Mpeg2Video::Get(int &Count, ePictureType *PictureType) {
	static ePictureType slice_types[8] = { NO_PICTURE, I_FRAME, P_FRAME, B_FRAME, NO_PICTURE, NO_PICTURE, NO_PICTURE, NO_PICTURE };
	if (start) {
		Count = start;
		*PictureType = NO_PICTURE;
		if (pictureStart >= 0)
			*PictureType = slice_types[(_data[pictureStart + 5] >> 3) & 0x07];
		return _data;
	}
	return nullptr;
}


//////////////////////////////////////////////////////////////////////////
// cTs2H264Video
//////////////////////////////////////////////////////////////////////////

class cTs2H264Video : public cTs2Video {
public:
	virtual bool PutTs(const uint8_t *Data) override;
	virtual uint8_t *Get(int &Count, ePictureType *PictureType) override;
private:
	enum {
		SYNCING,
		FIND_PICTURE,
		SCAN_PICTURE,
	} state = SYNCING;
};

bool cTs2H264Video::PutTs(const uint8_t *Data) {
	int Length;

	if (TsPayloadStart(Data)) {
		Length = TsGetPayload(&Data);
		int PesLength = ::PesLength(Data) - 6;
		if (PesLength)
			__debugbreak();
		int o = PesPayloadOffset(Data);
		Data += o;
		Length -= o;
	} else
		Length = TsGetPayload(&Data);

	if (start + length + Length > size) {
		int newSize = std::max(KILOBYTE(2), (start + length + Length + KILOBYTE(2) - 1) & ~(KILOBYTE(2) - 1));
		if (auto *newData = (uint8_t *)realloc(_data, newSize)) {
			_data = newData;
			size = newSize;
		} else {
			esyslog("ERROR: out of memory");
			//			Reset();
			__debugbreak();
			return false;
		}
	}

	memcpy(data + length, Data, Length);
	length += Length;

	static uint8_t search[] = { 0x00, 0x00, 0x01, 0x09 };
	if (state == SYNCING) {
		uint8_t *found = data;
		uint8_t *end = data + length;
		if(length > 3) {
			if (found = (uint8_t*)memmem(found, end - found, search, 4)) {
				length = end - found;
				memmove(data, found, length);
				state = SCAN_PICTURE;
			} else {
				memmove(data, data + length - 3, 3);
				length = 3;
			}
		}
	} else {
		uint8_t *found = data + std::max(length - Length - 3, 0);
		uint8_t *end = data + length;
		if (found = (uint8_t*)memmem(found, end - found, search, 4)) {
			start = found - _data;
			length = end - found;
			pictureStart = 0;

			found = _data;
			end = _data + start - 1;

			while ((found = (uint8_t*)memmem(found, end - found, search, 3))) {
//				printf("%02X ", found[3] & 0x1F);
				switch (found[3] & 0x1F) {
				case 0x01: // picture start code
				case 0x05: // IDR start code
					pictureStart = found - _data;
					found = end - 1;
					break;
				}
				++found;
			}
//			printf(" %d\n", start);
			if (!pictureStart)
				__debugbreak();


		}
	}
	return start != 0;
}

#include "h264.h"
uint8_t * cTs2H264Video::Get(int &Count, ePictureType *PictureType) {
	static ePictureType slice_types[] = { P_FRAME, B_FRAME, I_FRAME, SP_FRAME, SI_FRAME, P_FRAME, B_FRAME, I_FRAME, SP_FRAME, SI_FRAME };
	if (start) {
		Count = start;
		*PictureType = NO_PICTURE;
		uint8_t nal_type = _data[pictureStart + 3] & 0x1f;
		if (pictureStart >= 0) {
			cH264Stream stream(_data + pictureStart + 4, start - pictureStart - 4);
			stream.ue(); // first_mb_in_slice -> address of the first macroblock
			auto slice_type = stream.ue();
			if (slice_type <= 9)
				*PictureType = slice_types[slice_type];
		}
		return _data;
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// cOsdPipReceiver
//////////////////////////////////////////////////////////////////////////
cSoftwarePipReceiver::cSoftwarePipReceiver(cRingBufferFrame *ESBuffer) {
	m_TSBuffer = new cRingBufferLinear(MEGABYTE(3), TS_SIZE * 2, true);
	m_TSBuffer->SetTimeouts(0, 100);
	m_ESBuffer = ESBuffer;
	m_Active = false;
	m_thread = std::thread(&cSoftwarePipReceiver::Action, this);

}

cSoftwarePipReceiver::~cSoftwarePipReceiver() {
	m_Active = false;
	m_thread.join();
	delete m_Ts2Video;
	delete m_TSBuffer;
}

void cSoftwarePipReceiver::Receive(uint8_t *Data, int Length) {
	int put = m_TSBuffer->Put(Data, Length);
	if (put != Length)
		m_TSBuffer->ReportOverflow(Length - put);
}

int cSoftwarePipReceiver::Read(int FileHandle, int Max) {
	return m_TSBuffer->Read(FileHandle, Max);

}

int cSoftwarePipReceiver::Read(FILE *FileHandle, int Max) {
	return m_TSBuffer->Read(FileHandle, Max);

}

void cSoftwarePipReceiver::Action() {
//	dsyslog("osdpip: receiver thread started (pid=%d)", std::this_thread::get_id().);

	m_Active = true;

	ePictureType PictureType = NO_PICTURE;
	bool syncing = true;
	while (m_Active) {
		int r;
		const uint8_t *b = m_TSBuffer->Get(r);
		if (b) {
			int Count = PutTsRaw(b, r); //->Put(b, r);
			if (Count)
				m_TSBuffer->Del(Count);
		}

		int Result;
		const uint8_t *p = m_Ts2Video ? m_Ts2Video->Get(Result, &PictureType) : nullptr;
		if (p) {
			if (!syncing || PictureType == I_FRAME) {
				syncing = false;
				if (PictureType != NO_PICTURE) {
					if ((SoftwarePipSetup.FrameMode(isH264()) == kFrameModeI && PictureType == I_FRAME) ||
						(SoftwarePipSetup.FrameMode(isH264()) == kFrameModeIP && (PictureType == I_FRAME || PictureType == P_FRAME)) ||
						(SoftwarePipSetup.FrameMode(isH264()) == kFrameModeIPB)) {
						auto *frame = new cFrame(p, Result, PictureType);
						if (!m_ESBuffer->Put(frame)) {
							delete frame;
						}
					}
				}
			}
			m_Ts2Video->Del();
		}
	}

//	dsyslog("osdpip: receiver thread ended (pid=%d)", m_thread.native_handle());
}

#define TS_SYNC_BYTE 0x47
int cSoftwarePipReceiver::PutTsRaw(const uint8_t *Data, int Count) {
	int used = 0;

	// Make sure we are looking at a TS packet:

	while (Count > TS_SIZE) {
		if (Data[0] == TS_SYNC_BYTE && Data[TS_SIZE] == TS_SYNC_BYTE)
			break;
		Data++;
		Count--;
		used++;
	}

	if (used)
		esyslog("ERROR: skipped %d byte to sync on TS packet", used);

	for (int i = 0; i < Count; i += TS_SIZE) {
		if (Count - i < TS_SIZE)
			break;
		if (Data[i] != TS_SYNC_BYTE)
			break;

		int pid = TsPid(Data + i);
		if (vpid == -1) {
			const uint8_t *Pes = Data + i;
			int PesLength = 0;
			if (TsPayloadStart(Data + i) && PesLongEnough(PesLength = TsGetPayload(&Pes))) {
				if (*Pes == 0x00 && Pes[1] == 0x00 && Pes[2] == 0x01 && Pes[3] == 0xe0) {
					int PesPayloadOffset = ::PesPayloadOffset(Pes);
					if (TS_SIZE - TsPayloadOffset(Data + i) - PesPayloadOffset < 5)
						break;
					h264 = (!Pes[PesPayloadOffset + 0] && !Pes[PesPayloadOffset + 1] && (
								(Pes[PesPayloadOffset + 2] == 0x00 && Pes[PesPayloadOffset + 3] == 0x01 && Pes[PesPayloadOffset + 4] == 0x09) ||
								(Pes[PesPayloadOffset + 2] == 0x01 && Pes[PesPayloadOffset + 3] == 0x09)));
					vpid = pid;
					if (h264) {
						m_Ts2Video = new cTs2H264Video;
					} else {
						m_Ts2Video = new cTs2Mpeg2Video;
					}
				}
			}
		}
		if (TsHasPayload(Data + i) && (vpid == pid)) { // got payload
			if(m_Ts2Video->PutTs(Data + i))
				return used + TS_SIZE;
		}
		used += TS_SIZE;
	}
	return used;
}

