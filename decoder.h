/*
* OSD Picture in Picture plugin for the Video Disk Recorder
*
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

#ifndef VDR_OSDPIP_DECODER_H
#define VDR_OSDPIP_DECODER_H

extern "C"
{
#include <libavcodec/avcodec.h>
//#include <libavcodec/opt.h>
}
#include "bmp.h"

class cDecoder {
private:
//	AVCodec * m_Codec = nullptr;
	AVCodecContext * m_Context = nullptr;
	AVFrame * m_PicDecodedTmp = nullptr;
	AVFrame * m_PicDecoded = nullptr;
	AVFrame * m_PicResample = nullptr;
	AVCodecID m_CodecID = AV_CODEC_ID_NONE;
	unsigned char * m_BufferResample = nullptr;
	size_t m_BufferResampleSize = 0;
	int m_Width = 0;
	int m_Height = 0;
	cBmp	bmp;
public:
	int Open(bool h264);
	bool isOpen() { return m_CodecID != AV_CODEC_ID_NONE; }
	bool isOpenFor(bool h264) { return m_CodecID == (h264 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO); }
	int Close();
	int sendPacket(AVPacket* avpkt, bool drop, bool h264);
	int receiveFrame();
	void flush() { avcodec_flush_buffers(m_Context); }
	int Resample(int width, int height);
	AVFrame * PicResample() { return m_PicResample; }
	AVFrame *PicDecoded() { return m_PicDecoded; }
	double AspectRatio();
	int getHeightFromWidth(int width);
	int saveBMP(FILE *out) { return bmp.save(out, m_PicResample->data[0]); }
};

#endif // VDR_OSDPIP_DECODER_H
