#include "h264.h"
#include <cstdlib>

//#define BITAT(x, n) ((x & (1 << n)) == (1 << n))
#define BITAT(x, n) ((x & (1 << n)) != 0)


uint32_t cH264Stream::stream_read_bits(uint32_t n) {
	uint32_t ret = 0;

	if (n == 0) {
		return 0;
	}

	for (uint32_t i = 0; i < n; ++i) {
		if (stream_bits_remaining() == 0) {
			ret <<= n - i - 1;
		}
		uint8_t b = data[byte_pos];
		if (n - i <= 32) {
			ret = ret << 1 | BITAT(b, bit_pos);
		}
		if (bit_pos == 0) {
			bit_pos = 7;
			byte_pos++;
		} else {
			bit_pos--;
		}
	}
	return ret;
}

uint32_t cH264Stream::stream_peek_bits(uint32_t n) {
	uint8_t prev_bit_pos = bit_pos;
	uint32_t prev_byte_pos = byte_pos;
	uint32_t ret = stream_read_bits(n);
	bit_pos = prev_bit_pos;
	byte_pos = prev_byte_pos;
	return ret;
}
uint32_t cH264Stream::stream_read_bytes(uint32_t n) {
	uint32_t ret = 0;

	if (n == 0) {
		return 0;
	}

	for (uint32_t i = 0; i < n; ++i) {
		if (stream_bytes_remaining() == 0) {
			ret <<= (n - i - 1) * 8;
			break;
		}
		if (n - i <= 4) {
			ret = ret << 8 | data[byte_pos];
		}
		byte_pos++;
	}

	return ret;
}
uint32_t cH264Stream::stream_peek_bytes(uint32_t n) {
	uint32_t prev_byte_pos = byte_pos;
	uint32_t ret = stream_read_bytes(n);
	byte_pos = prev_byte_pos;
	return ret;
}
int cH264Stream::stream_bits_remaining() { return (size - byte_pos) * 8 + bit_pos; }
int cH264Stream::stream_bytes_remaining() { return size - byte_pos; }

uint32_t cH264Stream::h264_next_bits(uint32_t n) {
	if (n % 8 == 0) {
		return stream_peek_bytes(n / 8);
	}
	return stream_peek_bits(n);
}

uint32_t cH264Stream::u(uint32_t n) {
	//if (n % 8 == 0) {
	//    return h264_stream_read_bytes(s, n / 8);
	//}
	return stream_read_bits(n);
}

static uint8_t h264_exp_golomb_bits[256] = {
	8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
uint32_t cH264Stream::ue() {
	uint32_t bits = 0, read;
	uint8_t coded;
	while (1) {
		if (stream_bytes_remaining() < 1) {
			read = stream_peek_bits(bit_pos) << (8 - bit_pos);
			break;
		} else {
			read = stream_peek_bits(8);
			if (bits > 16) {
				break;
			}
			if (read == 0) {
				stream_read_bits(8);
				bits += 8;
			} else {
				break;
			}
		}
	}
	coded = h264_exp_golomb_bits[read];
	stream_read_bits(coded);
	bits += coded;
	return stream_read_bits(bits + 1) - 1;
}


int32_t cH264Stream::se() {
	uint32_t ret = ue();
	if (!(ret & 0x1)) {
		ret >>= 1;
		return -(int32_t)ret;
	}
	return (ret + 1) >> 1;
}

#define h264_f(n, pattern) do { if(u(n) != pattern) exit(0); } while(0)

void cH264Stream::rbsp_trailing_bits() {
	h264_f(1, 1);
	h264_f(bit_pos, 0);
}