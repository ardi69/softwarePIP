// mpeg2bmp.cpp : Definiert den Einstiegspunkt für die Konsolenanwendung.
//
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include "software_pip.h"
extern "C" {
#	include <libavcodec/avcodec.h>
#ifndef _MSC_VER
	extern AVCodec ff_h264_decoder;
	extern AVCodec ff_mpeg2video_decoder;
#endif
}
#include <thread>
#include <chrono>

#define USE_POPEN 0
#if 1
#define TS_FILE "h264.ts"
#else
#define TS_FILE "mpeg2.ts"
#endif

#ifdef _MSC_VER
#include <fcntl.h>
#define POPEN_READ_MODE "rb"
#define popen _popen
#else
#define POPEN_READ_MODE "r"
#endif // _MSC_VER

int main(int argc, char *argv[])
{
#ifdef _MSC_VER
	avcodec_register_all();
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
#else
	avcodec_register(&ff_h264_decoder);
	avcodec_register(&ff_mpeg2video_decoder);
#endif

	const char *uri = "http://openatv:8001/1:0:19:283D:3FB:1:C00000:0:0:0:";
	if (argc >= 3 && strcmp(argv[1], "-i") == 0)
		uri = argv[2];
	char cmd[256] = "wget -q -O - ";
	strcat(cmd, uri);
#if !defined _MSC_VER || USE_POPEN
	FILE *inStream = popen(cmd, POPEN_READ_MODE);
#else
	FILE *inStream = fopen(TS_FILE, POPEN_READ_MODE);
#endif
		// fopen(TS_FILE, "rb");
	if (inStream) {
		cSoftwarePipObject Pip(fileno(inStream));
		Pip.Show();
		int ret;
		do {
			ret = Pip.Read(inStream, 1024*8);
#if defined _MSC_VER && !USE_POPEN
			std::this_thread::sleep_for(std::chrono::microseconds(10));
#endif
		} while (ret > 0 || (ret != 0 && errno == EAGAIN));
#ifdef _MSC_VER
	do
	{
	} while (1);
#endif
	}
	__debugbreak();
    return 0;
}

