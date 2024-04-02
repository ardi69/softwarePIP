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

#include <stdio.h>
#include <stdlib.h>
#include "decoder.h"
#include "setup.h"
#include "tools.h"

extern "C" {
#include <libswscale/swscale.h>
#include "libavutil/imgutils.h"
}

int cDecoder::Open(bool h264)
{
	bool reOpen = isOpen();
	if (reOpen) {
		if (isOpenFor(h264)) return 0; // already open
		m_CodecID = AV_CODEC_ID_NONE;
		avcodec_close(m_Context);
		avcodec_free_context(&m_Context);
	}
	AVCodec *Codec = avcodec_find_decoder(h264 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO);
	if (!Codec) {
		esyslog("codec not found");
		return -1;
	}
	m_Context = avcodec_alloc_context3(Codec);// avcodec_alloc_context();
	if (!m_Context) {
		esyslog("could not allocate context");
		return -1;
	} else if (avcodec_open2(m_Context, Codec, nullptr) < 0) {
		esyslog("could not open codec");
		avcodec_free_context(&m_Context);
		return -1;
	}
	if (!reOpen) {
		m_PicDecodedTmp = av_frame_alloc();
		m_PicDecoded = av_frame_alloc();
		m_PicResample = av_frame_alloc();
	}
	m_CodecID = h264 ? AV_CODEC_ID_H264 : AV_CODEC_ID_MPEG2VIDEO;

	return 0;
}

int cDecoder::Close() {
	av_free(m_BufferResample);
	av_frame_free(&m_PicResample);
	av_frame_free(&m_PicDecoded);
	av_frame_free(&m_PicDecodedTmp);
	avcodec_close(m_Context);
	avcodec_free_context(&m_Context);

	return 0;
}

int cDecoder::sendPacket(AVPacket* avpkt, bool drop, bool h264) {

	if (!isOpenFor(h264) && Open(h264) < 0) return -1;
	if (drop) avpkt->flags |= AV_PKT_FLAG_DISCARD;
	int ret = avcodec_send_packet(m_Context, avpkt);
 	if (ret == AVERROR_EOF) {
 		avcodec_flush_buffers(m_Context);
 		ret = avcodec_send_packet(m_Context, avpkt);
 	}
	if(SoftwarePipSetup.FrameMode(h264) == kFrameModeI) avcodec_send_packet(m_Context, nullptr);
	if (ret != 0 && ret != AVERROR(EAGAIN))
		esyslog("avcodec_send_packet Error:%d", ret);
	return ret;
}

int cDecoder::receiveFrame() {
	int ret = -1, r;
	int pictures = 0;
	while ((r = avcodec_receive_frame(m_Context, m_PicDecodedTmp)) == 0) {
		av_frame_unref(m_PicDecoded);
		av_frame_ref(m_PicDecoded, m_PicDecodedTmp);
		++pictures;
		ret = 0;
	}
	if (pictures > 1) isyslog("discard %d pictures from rescale", pictures - 1);
	if (r != 0 && r != AVERROR(EAGAIN) && r != AVERROR_EOF)
		esyslog("avcodec_receive_frame Error:%d", r);
	return ret == 0 ? ret : r;
}

int cDecoder::Resample(int width, int height) {
	bool resizeBuffer = m_Width != width || m_Height != height;
	m_Width = width;
	m_Height = height;

	struct SwsContext * context;
	context = sws_getContext(m_Context->width, m_Context->height, m_Context->pix_fmt, m_Width, m_Height, AV_PIX_FMT_BGR24, SWS_LANCZOS, NULL, NULL, NULL);
	if (!context) {
		esyslog("Error initializing scale context.");
		return -1;
	}
	if (resizeBuffer) {
		bmp.resize(m_Width, m_Height);
		size_t size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, m_Width, m_Height, 4);
		if (size > m_BufferResampleSize) {
			void *newBuff = av_realloc(m_BufferResample, size);
			if (!newBuff) return -1;
			m_BufferResampleSize = size;
			m_BufferResample = (uint8_t*)newBuff;
		}
	}
	av_image_fill_arrays(m_PicResample->data, m_PicResample->linesize, m_BufferResample, AV_PIX_FMT_BGR24, m_Width, m_Height, 4);

	// horizontal flip for bmp
	m_PicResample->data[0] += m_PicResample->linesize[0] * (m_Height - 1);
	m_PicResample->linesize[0] = -m_PicResample->linesize[0];

	sws_scale(context, m_PicDecoded->data, m_PicDecoded->linesize,
			  0, m_Context->height,
			  m_PicResample->data, m_PicResample->linesize);

	// restore arrays
	m_PicResample->linesize[0] = -m_PicResample->linesize[0];
	m_PicResample->data[0] -= m_PicResample->linesize[0] * (m_Height - 1);


	sws_freeContext(context);

	return 0;
}

double cDecoder::AspectRatio() {
	return av_q2d(m_Context->sample_aspect_ratio) * (double)m_Context->width / (double)m_Context->height;
}

int cDecoder::getHeightFromWidth(int width) {
	if (!m_Context->sample_aspect_ratio.den || !m_Context->sample_aspect_ratio.num) return (((width * 3 * 2) / 4) + 1) / 2;
	return (((width * m_Context->height * m_Context->sample_aspect_ratio.den * 2) / (m_Context->sample_aspect_ratio.num * m_Context->width)) + 1) >> 1;
}

