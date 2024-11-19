#include "HexDump.hh"
#include "narrow.hh"
#include "strCat.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>

namespace HexDump {

using openmsx::MemBuffer;

[[nodiscard]] static constexpr char encode2(uint8_t x)
{
	return (x < 10) ? char(x + '0') : char(x - 10 + 'A');
}
[[nodiscard]] static auto encode(uint8_t x)
{
	return tmpStrCat(encode2(x >> 4), encode2(x & 15));
}
std::string encode(std::span<const uint8_t> input, bool newlines)
{
	std::string ret;
	size_t in = 0;
	auto len = input.size();
	while (len) {
		if (newlines && !ret.empty()) ret += '\n';
		auto t = int(std::min<size_t>(16, len));
		for (auto i : xrange(t)) {
			ret += encode(input[in++]);
			if (i != (t - 1)) ret += ' ';
		}
		len -= t;
	}
	return ret;
}

[[nodiscard]] static constexpr int decode(char x)
{
	if (('0' <= x) && (x <= '9')) {
		return x - '0';
	} else if (('A' <= x) && (x <= 'F')) {
		return x - 'A' + 10;
	} else if (('a' <= x) && (x <= 'f')) {
		return x - 'a' + 10;
	} else {
		return -1;
	}
}
MemBuffer<uint8_t> decode(std::string_view input)
{
	auto inSize = input.size();
	auto outSize = inSize / 2; // overestimation
	MemBuffer<uint8_t> ret(outSize); // too big

	size_t out = 0;
	bool flip = true;
	uint8_t tmp = 0;
	for (char c : input) {
		int d = decode(c);
		if (d == -1) continue;
		assert(d >= 0);
		assert(d <= 15);
		if (flip) {
			tmp = narrow<uint8_t>(d);
		} else {
			ret[out++] = narrow<uint8_t>((tmp << 4) | d);
		}
		flip = !flip;
	}

	assert(outSize >= out);
	ret.resize(out); // shrink to correct size
	return ret;
}

bool decode_inplace(std::string_view input, std::span<uint8_t> output)
{
	size_t out = 0;
	auto outSize = output.size();
	bool flip = true;
	uint8_t tmp = 0;
	for (char c : input) {
		int d = decode(c);
		if (d == -1) continue;
		assert(d >= 0);
		assert(d <= 15);
		if (flip) {
			tmp = narrow<uint8_t>(d);
		} else {
			if (out == outSize) [[unlikely]] return false;
			output[out++] = narrow<uint8_t>((tmp << 4) | d);
		}
		flip = !flip;
	}

	return out == outSize;
}

} // namespace HexDump
