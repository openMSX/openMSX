#include "Base64.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "xrange.hh"
#include <algorithm>
#include <array>
#include <cassert>

namespace Base64 {

using openmsx::MemBuffer;

[[nodiscard]] static constexpr char encode(uint8_t c)
{
	constexpr std::array<char, 64> base64_chars = {
		'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
		'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
		'0','1','2','3','4','5','6','7','8','9','+','/',
	};
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

std::string encode(std::span<const uint8_t> input)
{
	constexpr int CHUNKS = 19;
	constexpr int IN_CHUNKS  = 3 * CHUNKS;
	constexpr int OUT_CHUNKS = 4 * CHUNKS; // 76 chars per line

	auto outSize = ((input.size() + (IN_CHUNKS - 1)) / IN_CHUNKS) * (OUT_CHUNKS + 1); // overestimation
	std::string ret(outSize, 0); // too big

	size_t out = 0;
	while (!input.empty()) {
		if (out) ret[out++] = '\n';
		auto n = std::min<size_t>(IN_CHUNKS, input.size());
		for (/**/; n >= 3; n -= 3) {
			ret[out++] = encode(uint8_t( (input[0] & 0xfc) >> 2));
			ret[out++] = encode(uint8_t(((input[0] & 0x03) << 4) +
			                            ((input[1] & 0xf0) >> 4)));
			ret[out++] = encode(uint8_t(((input[1] & 0x0f) << 2) +
			                            ((input[2] & 0xc0) >> 6)));
			ret[out++] = encode(uint8_t( (input[2] & 0x3f) >> 0));
			input = input.subspan(3);
		}
		if (n) {
			std::array<uint8_t, 3> buf3 = {0, 0, 0};
			ranges::copy(input.subspan(0, n), buf3);
			input = input.subspan(n);

			std::array<uint8_t, 4> buf4;
			buf4[0] = uint8_t( (buf3[0] & 0xfc) >> 2);
			buf4[1] = uint8_t(((buf3[0] & 0x03) << 4) +
			                  ((buf3[1] & 0xf0) >> 4));
			buf4[2] = uint8_t(((buf3[1] & 0x0f) << 2) +
			                  ((buf3[2] & 0xc0) >> 6));
			buf4[3] = uint8_t( (buf3[2] & 0x3f) >> 0);
			for (auto j : xrange(n + 1)) {
				ret[out++] = encode(buf4[j]);
			}
			for (/**/; n < 3; ++n) {
				ret[out++] = '=';
			}
		}
	}

	assert(outSize >= out);
	ret.resize(out); // shrink to correct size
	return ret;
}

MemBuffer<uint8_t> decode(std::string_view input)
{
	auto outSize = (input.size() * 3 + 3) / 4; // overestimation
	MemBuffer<uint8_t> ret(outSize); // too big

	unsigned i = 0;
	size_t out = 0;
	std::array<uint8_t, 4> buf4;
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
		std::array<uint8_t, 3> buf3;
		buf3[0] = narrow_cast<uint8_t>(((buf4[0] & 0xff) << 2) + ((buf4[1] & 0x30) >> 4));
		buf3[1] = narrow_cast<uint8_t>(((buf4[1] & 0x0f) << 4) + ((buf4[2] & 0x3c) >> 2));
		buf3[2] = narrow_cast<uint8_t>(((buf4[2] & 0x03) << 6) + ((buf4[3] & 0xff) >> 0));
		for (auto j : xrange(i - 1)) {
			ret[out++] = buf3[j];
		}
	}

	assert(outSize >= out);
	ret.resize(out); // shrink to correct size
	return ret;
}

bool decode_inplace(std::string_view input, std::span<uint8_t> output)
{
	auto outSize = output.size();
	unsigned i = 0;
	size_t out = 0;
	std::array<uint8_t, 4> buf4;
	for (auto c : input) {
		uint8_t d = decode(c);
		if (d == uint8_t(-1)) continue;
		buf4[i++] = d;
		if (i == 4) {
			i = 0;
			if ((out + 3) > outSize) [[unlikely]] return false;
			output[out++] = char(((buf4[0] & 0xff) << 2) + ((buf4[1] & 0x30) >> 4));
			output[out++] = char(((buf4[1] & 0x0f) << 4) + ((buf4[2] & 0x3c) >> 2));
			output[out++] = char(((buf4[2] & 0x03) << 6) + ((buf4[3] & 0xff) >> 0));
		}
	}
	if (i) {
		for (auto j : xrange(i, 4u)) {
			buf4[j] = 0;
		}
		std::array<uint8_t, 3> buf3;
		buf3[0] = narrow_cast<uint8_t>(((buf4[0] & 0xff) << 2) + ((buf4[1] & 0x30) >> 4));
		buf3[1] = narrow_cast<uint8_t>(((buf4[1] & 0x0f) << 4) + ((buf4[2] & 0x3c) >> 2));
		buf3[2] = narrow_cast<uint8_t>(((buf4[2] & 0x03) << 6) + ((buf4[3] & 0xff) >> 0));
		for (auto j : xrange(i - 1)) {
			if (out == outSize) [[unlikely]] return false;
			output[out++] = buf3[j];
		}
	}

	return out == outSize;
}

} // namespace Base64
