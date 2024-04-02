#ifndef h264_h__
#define h264_h__

#include <cstdint>

class cH264Stream {
	uint8_t *data;
	uint32_t size;
	uint8_t bit_pos = 7;
	uint32_t byte_pos = 0;
public:
	cH264Stream(uint8_t *Data, uint32_t Size) : data(Data), size(Size) {}

	uint32_t stream_read_bits(uint32_t n);
	uint32_t stream_peek_bits(uint32_t n);
	uint32_t stream_read_bytes(uint32_t n);
	uint32_t stream_peek_bytes(uint32_t n);
	int stream_bits_remaining();
	int stream_bytes_remaining();


	uint32_t h264_next_bits(uint32_t n);
	uint32_t u(uint32_t n);
	uint32_t ue();
	int32_t se();
	void rbsp_trailing_bits();
};





#endif // h264_h__
