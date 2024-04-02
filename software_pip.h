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

#ifndef VDR_OSDPIP_OSD_H
#define VDR_OSDPIP_OSD_H

#include "receiver.h"
#include "decoder.h"
#include <thread>

//class cRingBufferFrame;
//class cOsdPipReceiver;

class cSoftwarePipObject {
private:
	cRingBufferFrame *m_ESBuffer;
	cSoftwarePipReceiver *m_Receiver;
	int m_handle;
	bool m_Active;
	bool m_Ready;
	bool m_Reset;
	int m_Width, m_Height;
	int m_FrameDrop;

	unsigned int m_AlphaBase;
	unsigned int m_Palette[256];
	int m_PaletteStart;

	cDecoder decoder;
	std::thread m_thread;

	void ProcessImage(AVPacket* data);

	void Stop(void);
	void SwitchOsdPipChan(int i);
protected:
	void Action(void);

public:
	cSoftwarePipObject(int FileHanlde);
	~cSoftwarePipObject(void);
	void Show(void);
	void Receive(uint8_t *Data, int Length) {
		m_Receiver->Receive(Data, Length);
	}
	int Read(int Length) {
		return m_Receiver->Read(m_handle, Length);
	}
	int Read(FILE *inStream, int Length) {
		return m_Receiver->Read(inStream, Length);
	}

};

#endif // VDR_OSDPIP_OSD_H
