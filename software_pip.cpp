/*
* OSD Picture in Picture plugin for the Video Disk Recorder
*
* Copyright (C) 2010        Mitchm at vdrportal.de
* Copyright (C) 2007        Martin Wache (dithered 256 color mode)
* Copyright (C) 2004 - 2008 Andreas Regel <andreas.regel@powarman.de>
* Copyright (C) 2004 - 2005 Sascha Volkenandt <sascha@akv-soft.de>
*
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

#include <stdint.h>

#include "software_pip.h"
#include "decoder.h"
#include "receiver.h"
#include "setup.h"

#include "ringbuffer.h"
#include "tools.h"
#include <thread>


cSoftwarePipObject::cSoftwarePipObject(int handle) {
	m_ESBuffer = new cRingBufferFrame(MEGABYTE(3), true);
	m_ESBuffer->SetTimeouts(0, 1);
	m_handle = handle;
	m_Active = false;
	m_Ready = false;
	m_Reset = true;
	m_Width = m_Height = -1;
	m_Width = 260; m_Height = 208;


	m_AlphaBase = 0xFF000000;
	for (int i = 0; i < 256; i++)
		m_Palette[i] = m_AlphaBase | i;
	m_PaletteStart = 1;

//	Device->SwitchChannel(m_Channel, false);
	m_Receiver = new cSoftwarePipReceiver(m_ESBuffer);
//	Device->AttachReceiver(m_Receiver);
}

cSoftwarePipObject::~cSoftwarePipObject() {
	Stop();

	delete m_Receiver;
	delete m_ESBuffer;
}

void cSoftwarePipObject::Stop(void) {
	if (m_Active) {
		m_Active = false;
		m_thread.join();
	}
	m_ESBuffer->Clear();
}

#if 0
void cOsdPipObject::SwitchOsdPipChan(int i) {
	const cChannel *pipChan = m_Channel;
	pipChan = Channels.GetByNumber(m_Channel->Number() + i);
	if (pipChan) {
		Stop();
		DELETENULL(m_Receiver);
		cDevice *dev = cDevice::GetDevice(pipChan, 1, false);
		if (dev) {
			m_Channel = pipChan;
			dev->SwitchChannel(m_Channel, false);
			m_InfoWindow->SetMessage(tr("Zapping mode"));
			m_InfoWindow->Show(true);
			m_Receiver = new cOsdPipReceiver(m_Channel, m_ESBuffer);
			dev->AttachReceiver(m_Receiver);
		}
		Start();
	}
}
#endif

static int pics;

void cSoftwarePipObject::ProcessImage(AVPacket* data) {
	int height;

	if (m_FrameDrop != -1) {
		if (SoftwarePipSetup.FrameMode(m_Receiver->isH264()) == kFrameModeI) {
			if (m_FrameDrop == SoftwarePipSetup.FrameDrop) {
				m_FrameDrop = 0;
			} else {
				m_FrameDrop++;
				return;
			}
		}
	}

//	if (decoder.Decode(data, m_Receiver->isH264()) != 0)
//		return;
	static std::chrono::system_clock::time_point last_pic;
	std::chrono::system_clock::time_point this_pic = std::chrono::system_clock::now();
	std::chrono::milliseconds diff;
	if (last_pic.time_since_epoch().count())
		diff = std::chrono::duration_cast<std::chrono::milliseconds>(this_pic - last_pic);
	else
		diff = std::chrono::milliseconds::zero();
	last_pic = this_pic;

	isyslog("show pic %d (%d) (%d ms --> %.2f) %s", decoder.PicDecoded()->coded_picture_number, pics, (int)diff.count(), 1000.0 / (int)diff.count(), decoder.PicDecoded()->key_frame ? " Key" : "");

	if (m_FrameDrop != -1) {
		if (SoftwarePipSetup.FrameMode(m_Receiver->isH264()) == kFrameModeIP ||
			SoftwarePipSetup.FrameMode(m_Receiver->isH264()) == kFrameModeIPB) {
			if (m_FrameDrop == SoftwarePipSetup.FrameDrop) {
				m_FrameDrop = 0;
			} else {
				m_FrameDrop++;
				return;
			}
		}
	}

	height = decoder.getHeightFromWidth(m_Width);
	if (decoder.Resample(m_Width, height) != 0)
		return;
	static int c = 0;
	isyslog("image sampled %d", ++c);

//	 FILE *out = fopen("test.bmp", "wb");
//	 if (out) {
#ifndef _MSC_VER
		 decoder.saveBMP(stdout);
#endif
//		 fclose(out);
//	 }
	 if (!m_Ready) {
		m_Ready = true;
	}
}

void cSoftwarePipObject::Action(void) {
	m_Active = true;

	isyslog("osdpip: decoder thread started (pid = %d)", 0/*getpid()*/);


	cFrame * frame;
	m_FrameDrop = SoftwarePipSetup.FrameDrop;

	while (m_Active) {
		if (m_Reset) {
			m_Ready = false;
			m_Reset = false;
		}
		if (m_FrameDrop == -1) {
			bool received = false;
			int dropped = 0, decoded = 0;
			while ((frame = m_ESBuffer->Get()) != NULL) {
				if (frame->Count() > 0) {
					if (frame->Type() == I_FRAME && m_ESBuffer->IFrames() > 2) {
						do {
							m_ESBuffer->Drop(frame);
							frame = m_ESBuffer->Get();
						} while (frame->Type() != I_FRAME);
					}
					bool drop = m_ESBuffer->Available() > frame->Count();
//					if (OsdPipSetup.FrameMode == kFrameModeIP ||
//						OsdPipSetup.FrameMode == kFrameModeIPB) {
						if (frame->Type() != B_FRAME || !drop) {
							++decoded;
							int res = decoder.sendPacket(frame->Data(), (drop && frame->Type() != I_FRAME), m_Receiver->isH264());
							if (res == 0) ++pics;

							if (res == 0 || res == AVERROR(EAGAIN)) {
								res = decoder.receiveFrame();
								if (res == 0) {
									--pics;
									received = true;
									break;
								}
							}
						}
//					}
						if (drop && frame->Type() != I_FRAME) ++dropped;
				}
				m_ESBuffer->Drop(frame);
//				if (SoftwarePipSetup.FrameMode(m_Receiver->isH264()) == kFrameModeI && pics > 1) { decoder.flush(); pics = 0; printf("flush buffer\n"); }
			}
//			if (dropped || decoded) esyslog("%d Frames dropped (%d decoded)", dropped, decoded);
			if (received) {
				m_ESBuffer->Drop(frame);
				ProcessImage(frame->Data());
			}
		} else {
			frame = m_ESBuffer->Get();
			if (frame) {
				if (frame->Count() > 0) {
					decoder.sendPacket(frame->Data(), false, m_Receiver->isH264());
					if (decoder.receiveFrame() == 0)
						ProcessImage(frame->Data());
				}
				m_ESBuffer->Drop(frame);
			}
		}
//		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	decoder.Close();

	isyslog("osdpip: decoder thread stopped");
}

void cSoftwarePipObject::Show(void) {
	if(!m_Active)
		m_thread = std::thread(&cSoftwarePipObject::Action, this);
}



