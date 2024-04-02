#include "bmp.h"



int cBmp::save(FILE *out, uint8_t *data) {
	int err;
	fwrite(dynamic_cast<sBmpHeader*>(this), 1, bfOffBits, out);
	if ((err = ferror(out))) return err;
	fwrite(data, 1, biSizeImage, out);
	if ((err = ferror(out))) return err;
	fflush(out);
	if ((err = ferror(out))) return err;
	return 0;
}
