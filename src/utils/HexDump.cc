#include "HexDump.hh"
#include "likely.hh"
#include <algorithm>
#include <cassert>

namespace HexDump {

using std::string;
using openmsx::MemBuffer;

static char encode2(uint8_t x)
{
	return (x < 10) ? (x + '0') : (x - 10 + 'A');
}
static string encode(uint8_t x)
{
	string result;
	result += encode2(x >> 4);
	result += encode2(x & 15);
	return result;
}
string encode(const uint8_t* input, size_t len, bool newlines)
{
	string ret;
	while (len) {
		if (newlines && !ret.empty()) ret += '\n';
		int t = int(std::min<size_t>(16, len));
		for (int i = 0; i < t; ++i) {
			ret += encode(*input++);
			if (i != (t - 1)) ret += ' ';
		}
		len -= t;
	}
	return ret;
}

static int decode(char x)
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
std::pair<MemBuffer<uint8_t>, size_t> decode(string_ref input)
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
		if (flip) {
			tmp = d;
		} else {
			ret[out++] = (tmp << 4) | d;
		}
		flip = !flip;
	}

	assert(outSize >= out);
	ret.resize(out); // shrink to correct size
	return std::make_pair(std::move(ret), out);
}

bool decode_inplace(string_ref input, uint8_t* output, size_t outSize)
{
	size_t out = 0;
	bool flip = true;
	uint8_t tmp = 0;
	for (char c : input) {
		int d = decode(c);
		if (d == -1) continue;
		if (flip) {
			tmp = d;
		} else {
			if (unlikely(out == outSize)) return false;
			output[out++] = (tmp << 4) | d;
		}
		flip = !flip;
	}

	return out == outSize;
}

} // namespace HexDump
