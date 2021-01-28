#include "Base64.hh"
#include "likely.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>

namespace Base64 {

using std::string;
using openmsx::MemBuffer;

[[nodiscard]] static constexpr char encode(uint8_t c)
{
	constexpr const char* const base64_chars =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	assert(c < 64);
	return base64_chars[c];
}

[[nodiscard]] static constexpr uint8_t decode(uint8_t c)
{
	if        ('A' <= c && c <= 'Z') {
		return c - 'A';
	} else if ('a' <= c && c <= 'z') {
		return c - 'a' + 26;
	} else if ('0' <= c && c <= '9') {
		return c - '0' + 52;
	} else if (c == '+') {
		return 62;
	} else if (c == '/') {
		return 63;
	} else {
		return uint8_t(-1);
	}
}

string encode(const uint8_t* input, size_t inSize)
{
	constexpr int CHUNKS = 19;
	constexpr int IN_CHUNKS  = 3 * CHUNKS;
	constexpr int OUT_CHUNKS = 4 * CHUNKS; // 76 chars per line

	auto outSize = ((inSize + (IN_CHUNKS - 1)) / IN_CHUNKS) * (OUT_CHUNKS + 1); // overestimation
	string ret(outSize, 0); // too big

	size_t out = 0;
	while (inSize) {
		if (out) ret[out++] = '\n';
		auto n2 = std::min<size_t>(IN_CHUNKS, inSize);
		auto n = unsigned(n2);
		for (/**/; n >= 3; n -= 3) {
			ret[out++] = encode( (input[0] & 0xfc) >> 2);
			ret[out++] = encode(((input[0] & 0x03) << 4) +
			                    ((input[1] & 0xf0) >> 4));
			ret[out++] = encode(((input[1] & 0x0f) << 2) +
			                    ((input[2] & 0xc0) >> 6));
			ret[out++] = encode( (input[2] & 0x3f) >> 0);
			input += 3;
		}
		if (n) {
			uint8_t buf3[3] = { 0, 0, 0 };
			for (auto i : xrange(n)) {
				buf3[i] = input[i];
			}
			uint8_t buf4[4];
			buf4[0] =  (buf3[0] & 0xfc) >> 2;
			buf4[1] = ((buf3[0] & 0x03) << 4) +
				  ((buf3[1] & 0xf0) >> 4);
			buf4[2] = ((buf3[1] & 0x0f) << 2) +
				  ((buf3[2] & 0xc0) >> 6);
			buf4[3] =  (buf3[2] & 0x3f) >> 0;
			for (auto j : xrange(n + 1)) {
				ret[out++] = encode(buf4[j]);
			}
			for (/**/; n < 3; ++n) {
				ret[out++] = '=';
			}
		}
		inSize -= n2;
	}

	assert(outSize >= out);
	ret.resize(out); // shrink to correct size
	return ret;
}

std::pair<MemBuffer<uint8_t>, size_t> decode(std::string_view input)
{
	auto outSize = (input.size() * 3 + 3) / 4; // overestimation
	MemBuffer<uint8_t> ret(outSize); // too big

	unsigned i = 0;
	size_t out = 0;
	uint8_t buf4[4];
	for (auto c : input) {
		uint8_t d = decode(c);
		if (d == uint8_t(-1)) continue;
		buf4[i++] = d;
		if (i == 4) {
			i = 0;
			ret[out++] = char(((buf4[0] & 0xff) << 2) + ((buf4[1] & 0x30) >> 4));
			ret[out++] = char(((buf4[1] & 0x0f) << 4) + ((buf4[2] & 0x3c) >> 2));
			ret[out++] = char(((buf4[2] & 0x03) << 6) + ((buf4[3] & 0xff) >> 0));
		}
	}
	if (i) {
		for (auto j : xrange(i, 4u)) {
			buf4[j] = 0;
		}
		uint8_t buf3[3];
		buf3[0] = ((buf4[0] & 0xff) << 2) + ((buf4[1] & 0x30) >> 4);
		buf3[1] = ((buf4[1] & 0x0f) << 4) + ((buf4[2] & 0x3c) >> 2);
		buf3[2] = ((buf4[2] & 0x03) << 6) + ((buf4[3] & 0xff) >> 0);
		for (auto j : xrange(i - 1)) {
			ret[out++] = buf3[j];
		}
	}

	assert(outSize >= out);
	ret.resize(out); // shrink to correct size
	return std::pair(std::move(ret), out);
}

bool decode_inplace(std::string_view input, uint8_t* output, size_t outSize)
{
	unsigned i = 0;
	size_t out = 0;
	uint8_t buf4[4];
	for (auto c : input) {
		uint8_t d = decode(c);
		if (d == uint8_t(-1)) continue;
		buf4[i++] = d;
		if (i == 4) {
			i = 0;
			if (unlikely((out + 3) > outSize)) return false;
			output[out++] = char(((buf4[0] & 0xff) << 2) + ((buf4[1] & 0x30) >> 4));
			output[out++] = char(((buf4[1] & 0x0f) << 4) + ((buf4[2] & 0x3c) >> 2));
			output[out++] = char(((buf4[2] & 0x03) << 6) + ((buf4[3] & 0xff) >> 0));
		}
	}
	if (i) {
		for (auto j : xrange(i, 4u)) {
			buf4[j] = 0;
		}
		uint8_t buf3[3];
		buf3[0] = ((buf4[0] & 0xff) << 2) + ((buf4[1] & 0x30) >> 4);
		buf3[1] = ((buf4[1] & 0x0f) << 4) + ((buf4[2] & 0x3c) >> 2);
		buf3[2] = ((buf4[2] & 0x03) << 6) + ((buf4[3] & 0xff) >> 0);
		for (auto j : xrange(i - 1)) {
			if (unlikely(out == outSize)) return false;
			output[out++] = buf3[j];
		}
	}

	return out == outSize;
}

} // namespace Base64
