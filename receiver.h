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

#ifndef VDR_OSDPIP_RECEIVER_H
#define VDR_OSDPIP_RECEIVER_H

//#include <vdr/receiver.h>
#include <thread>
#include "tools.h"

// Picture types:
// #define NO_PICTURE 0
// #define I_FRAME    1
// #define P_FRAME    2
// #define B_FRAME    3


#define TS_SYNC_BYTE          0x47
#define TS_SIZE               188
#define TS_ERROR              0x80
#define TS_PAYLOAD_START      0x40
#define TS_TRANSPORT_PRIORITY 0x20
#define TS_PID_MASK_HI        0x1F
#define TS_SCRAMBLING_CONTROL 0xC0
#define TS_ADAPT_FIELD_EXISTS 0x20
#define TS_PAYLOAD_EXISTS     0x10
#define TS_CONT_CNT_MASK      0x0F
#define TS_ADAPT_DISCONT      0x80
#define TS_ADAPT_RANDOM_ACC   0x40 // would be perfect for detecting independent frames, but unfortunately not used by all broadcasters
#define TS_ADAPT_ELEM_PRIO    0x20
#define TS_ADAPT_PCR          0x10
#define TS_ADAPT_OPCR         0x08
#define TS_ADAPT_SPLICING     0x04
#define TS_ADAPT_TP_PRIVATE   0x02
#define TS_ADAPT_EXTENSION    0x01

inline bool TsHasPayload(const uint8_t *p) {
	return (p[3] & TS_PAYLOAD_EXISTS) != 0;
}

inline bool TsHasAdaptationField(const uint8_t *p) {
	return (p[3] & TS_ADAPT_FIELD_EXISTS) != 0;
}

inline bool TsPayloadStart(const uint8_t *p) {
	return (p[1] & TS_PAYLOAD_START) != 0;
}

inline bool TsError(const uint8_t *p) {
	return (p[1] & TS_ERROR) != 0;
}

inline int TsPid(const uint8_t *p) {
	return (p[1] & TS_PID_MASK_HI) * 256 + p[2];
}

inline bool TsIsScrambled(const uint8_t *p) {
	return (p[3] & TS_SCRAMBLING_CONTROL) != 0;
}

inline int TsPayloadOffset(const uint8_t *p) {
	int o = TsHasAdaptationField(p) ? p[4] + 5 : 4;
	return o <= TS_SIZE ? o : TS_SIZE;
}

inline int TsGetPayload(const uint8_t **p) {
	if (TsHasPayload(*p)) {
		int o = TsPayloadOffset(*p);
		*p += o;
		return TS_SIZE - o;
	}
	return 0;
}

inline int TsContinuityCounter(const uint8_t *p) {
	return p[3] & TS_CONT_CNT_MASK;
}

inline int TsGetAdaptationField(const uint8_t *p) {
	return TsHasAdaptationField(p) ? p[5] : 0x00;
}

// The following functions all take a pointer to a sequence of complete TS packets.

int64_t TsGetPts(const uint8_t *p, int l);
void TsSetTeiOnBrokenPackets(uint8_t *p, int l);

// Some PES handling tools:
// The following functions that take a pointer to PES data all assume that
// there is enough data so that PesLongEnough() returns true.

inline bool PesLongEnough(int Length) {
	return Length >= 6;
}

inline bool PesHasLength(const uint8_t *p) {
	return (p[4] | p[5]) != 0;
}

inline int PesLength(const uint8_t *p) {
	return 6 + p[4] * 256 + p[5];
}

inline int PesPayloadOffset(const uint8_t *p) {
	return 9 + p[8];
}

inline bool PesHasPts(const uint8_t *p) {
	return (p[7] & 0x80) && p[8] >= 5;
}

inline int64_t PesGetPts(const uint8_t *p) {
	return ((((int64_t)p[9]) & 0x0E) << 29) |
		(((int64_t)p[10]) << 22) |
		((((int64_t)p[11]) & 0xFE) << 14) |
		(((int64_t)p[12]) << 7) |
		((((int64_t)p[13]) & 0xFE) >> 1);
}


class cRingBufferLinear;
class cRingBufferFrame;
class cTs2Video;

class cSoftwarePipReceiver {
public:
	cSoftwarePipReceiver(cRingBufferFrame *ESBuffer);
	~cSoftwarePipReceiver();
	void Receive(uint8_t *Data, int Length);
	int Read(int FileHandle, int Max);
	int Read(FILE *FileHandle, int Max);
	bool isH264() { return h264; }

private:
	void Action();
	int PutTsRaw(const uint8_t *Data, int Length);
	bool h264 = false;
	int vpid = -1;
	cRingBufferLinear *m_TSBuffer;
	cRingBufferFrame *m_ESBuffer;
	cTs2Video *m_Ts2Video = nullptr;
	bool m_Active;
	std::thread m_thread;
};

#endif // VDR_OSDPIP_RECEIVER_H
