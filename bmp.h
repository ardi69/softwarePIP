#ifndef bmp_h__
#define bmp_h__

#include <cstdint>
#include <cstdio>

#pragma pack(push, 1)
struct sBmpHeader {
	sBmpHeader(int32_t width, int32_t height) { init(width, height); }
	void init(int32_t width, int32_t height) {
		biWidth = width;
		biHeight = height;
		biSizeImage = ((biWidth * biBitCount / 8 + 3) & ~0x3) * height;
		bfSize = 54 /*bfOffBits*/ + biSizeImage;
	}
		// file_header
	uint16_t	bfType			= 0x4D42;
	uint32_t	bfSize;
	uint32_t	bfReserved		= 0;
	uint32_t	bfOffBits		= 54;
	// information_header
	uint32_t	biSize			= 40;
	int32_t		biWidth;
	int32_t		biHeight;
	uint16_t	biPlanes		= 1;
	uint16_t	biBitCount		= 24;
	uint32_t	biCompression	= 0;
	uint32_t	biSizeImage;
	uint32_t	biXPelsPerMeter	= 0;
	uint32_t	biYPelsPerMeter	= 0;
	uint32_t	biClrUsed		= 0;
	uint32_t	biClrImportant	= 0;
};
#pragma pack(pop)


class cBmp : private sBmpHeader {
public:
	cBmp(int32_t width = 0, int32_t height = 0) : sBmpHeader(width, height) {}
	void resize(int32_t width, int32_t height) { sBmpHeader::init(width, height); }
	int save(FILE *out, uint8_t *data);

};




#endif // bmp_h__