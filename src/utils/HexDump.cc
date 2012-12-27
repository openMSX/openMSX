// $Id$

#include "HexDump.hh"

namespace HexDump {

using std::string;

typedef unsigned char byte;

static char encode2(byte x)
{
	return (x < 10) ? (x + '0') : (x - 10 + 'A');
}
static string encode(byte x)
{
	string result;
	result += encode2(x >> 4);
	result += encode2(x & 15);
	return result;
}
string encode(const void* input_, size_t len, bool newlines)
{
	auto input = static_cast<const byte*>(input_);
	string ret;
	while (len) {
		if (newlines && !ret.empty()) ret += '\n';
		int t = std::min<size_t>(16, len);
		for (int i = 0; i < t; ++i) {
			ret += encode(*input++);
			if (i != 15) ret += ' ';
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
string decode(const string& input)
{
	string ret;
	const size_t len = input.size();
	bool flip = true;
	char tmp = 0;
	for (size_t in = 0; in < len; ++in) {
		int d = decode(input[in]);
		if (d == -1) continue;
		if (flip) {
			tmp = d << 4;
		} else {
			tmp |= d;
			ret += tmp;
		}
		flip = !flip;
	}
	return ret;
}

} // namespace HexDump
