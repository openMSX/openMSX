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

string encode(const void* input_, int len, bool newlines)
{
	const byte* input = static_cast<const byte*>(input_);
	string ret;
	int n = 0;
	for (/**/; len > 0; len -= 45) {
		if (newlines && !ret.empty()) ret += '\n';
		n = std::min(45, len);
		for (/**/; n >= 3; n -= 3) {
			ret += encode( (input[0] & 0xfc) >> 2);
			ret += encode(((input[0] & 0x03) << 4) +
			              ((input[1] & 0xf0) >> 4));
			ret += encode(((input[1] & 0x0f) << 2) +
			              ((input[2] & 0xc0) >> 6));
			ret += encode( (input[2] & 0x3f) >> 0);
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
				ret += encode(buf4[j]);
			}
			for (/**/; n < 3; ++n) {
				ret += '=';
			}
		}
	}
	return ret;
}

string decode(const string& input)
{
	string ret;
	int i = 0;
	byte buf4[4];
	const int len = input.size();
	for (int in = 0; in < len; ++in) {
		byte d = decode(input[in]);
		if (d == (byte)-1) continue;
		buf4[i++] = d;
		if (i == 4) {
			ret += char(((buf4[0] & 0xff) << 2) + ((buf4[1] & 0x30) >> 4));
			ret += char(((buf4[1] & 0x0f) << 4) + ((buf4[2] & 0x3c) >> 2));
			ret += char(((buf4[2] & 0x03) << 6) + ((buf4[3] & 0xff) >> 0));
			i = 0;
		}
	}
	if (i) {
		for (int j = i; j < 4; ++j) {
			buf4[j] = 0;
		}
		byte buf3[3];
		buf3[0] = ((buf4[0] & 0xff) << 2) + ((buf4[1] & 0x30) >> 4);
		buf3[1] = ((buf4[1] & 0x0f) << 4) + ((buf4[2] & 0x3c) >> 2);
		buf3[2] = ((buf4[2] & 0x03) << 6) + ((buf4[3] & 0xff) >> 0);
		for (int j = 0; (j < i - 1); ++j) {
			ret += buf3[j];
		}
	}
	return ret;
}

} // namespace Base64
