// $Id$

#include "Base64.hh"
#include <cassert>

namespace Base64 {

using std::string;

typedef unsigned char byte;

static inline char encode(byte c)
{
	static const char* const base64_chars =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	assert(c < 64);
	return base64_chars[c];
}

static inline byte decode(unsigned char c)
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
		return (byte)-1;
	}
}

string encode(const void* input_, int inSize)
{
	const byte* input = static_cast<const byte*>(input_);
	unsigned outSize = ((inSize + 44) / 45) * 61; // overestimation
	string ret(outSize, 0); // too big

	int n = 0;
	unsigned out = 0;
	for (/**/; inSize > 0; inSize -= 45) {
		if (!ret.empty()) ret += '\n';
		n = std::min(45, inSize);
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
			byte buf3[3] = { 0, 0, 0 };
			for (int i = 0; i < n; ++i) {
				buf3[i] = input[i];
			}
			byte buf4[4];
			buf4[0] =  (buf3[0] & 0xfc) >> 2;
			buf4[1] = ((buf3[0] & 0x03) << 4) +
				  ((buf3[1] & 0xf0) >> 4);
			buf4[2] = ((buf3[1] & 0x0f) << 2) +
				  ((buf3[2] & 0xc0) >> 6);
			buf4[3] =  (buf3[2] & 0x3f) >> 0;
			for (int j = 0; (j < n + 1); ++j) {
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

string decode(const string& input)
{
	const size_t inSize = input.size();
	unsigned outSize = (inSize * 3 + 3) / 4; // overestimation
	string ret(outSize, 0); // too big

	unsigned i = 0;
	unsigned out = 0;
	byte buf4[4];
	for (size_t in = 0; in < inSize; ++in) {
		byte d = decode(input[in]);
		if (d == (byte)-1) continue;
		buf4[i++] = d;
		if (i == 4) {
			i = 0;
			ret[out++] = char(((buf4[0] & 0xff) << 2) + ((buf4[1] & 0x30) >> 4));
			ret[out++] = char(((buf4[1] & 0x0f) << 4) + ((buf4[2] & 0x3c) >> 2));
			ret[out++] = char(((buf4[2] & 0x03) << 6) + ((buf4[3] & 0xff) >> 0));
		}
	}
	if (i) {
		for (unsigned j = i; j < 4; ++j) {
			buf4[j] = 0;
		}
		byte buf3[3];
		buf3[0] = ((buf4[0] & 0xff) << 2) + ((buf4[1] & 0x30) >> 4);
		buf3[1] = ((buf4[1] & 0x0f) << 4) + ((buf4[2] & 0x3c) >> 2);
		buf3[2] = ((buf4[2] & 0x03) << 6) + ((buf4[3] & 0xff) >> 0);
		for (unsigned j = 0; (j < i - 1); ++j) {
			ret[out++] = buf3[j];
		}
	}

	assert(outSize >= out);
	ret.resize(out); // shrink to correct size
	return ret;
}

} // namespace Base64
