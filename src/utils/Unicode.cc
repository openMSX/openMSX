//$Id$

#include "Unicode.hh"
#include <iostream>

using std::string;
using std::wstring;

namespace openmsx {

namespace Unicode {

/* decodes a a string possibly containing UTF-8 sequences to a
 * string of wide characters or to a string of 8-bit characters.
 * In the 8-bit characters string, the characters >= 0x100 are
 * mapped to '?'
 * this implementation follows the guidelines in
 * http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
 * except for the following:
 *  - the two byte seqence 0xC0 0x80 for '\0' is allowed
 *  - surrogates are ignored
 *
 *
 * TODO: catch surrogates
 * TODO: respond to malformed sequences in a more appropriate way
 */

static void bad_utf(const string& msg)
{
	std::cerr << "Error in UTF-8 encoding: " << msg << std::endl;
}

string utf8ToAscii(const string& utf8)
{
	string res;
	wstring wres = utf8ToUnicode(utf8);
	for (wstring::const_iterator it = wres.begin(); it != wres.end(); /* */) {
		unsigned unicode = *it++;
		res.push_back(unicode < 0x100 ? char(unicode) : '?');
	}
	return res;
}

wstring utf8ToUnicode(const string& utf8)
{
	wstring res;
	for (string::const_iterator it = utf8.begin(); it != utf8.end(); /* */) {
		char first = *it++;
		switch (first & 0xC0) {
		case 0x80:
			bad_utf("unexpected continuation byte");
			res.push_back('?');
			break;
		case 0xC0:
			char nbyte, mask;
			unsigned uni;
			for (mask = 0x20, first -= 0xC0, nbyte = 2;
			     first & mask;
			     mask >>= 1, ++nbyte) {
				first -= mask;
			}
			if (nbyte > 6) {
				bad_utf("illegal byte");
				uni = 0xFFFD;
				nbyte = 0;
			} else {
				char i;
				for (i = 1, uni = first; i < nbyte; ++i) {
					if ((it == utf8.end()) ||
					    ((*it & 0xC0) != 0x80)) {
						bad_utf("incomplete sequence");
						uni = 0xFFFD;
						break;
					} else {
						unsigned char ch = *it++;
						uni = (uni << 6) + (ch & 0x3F);
					}
				}
				// check for overlong sequences
				if ((i == nbyte) &&
				    (uni < (1u << (5 * nbyte - (nbyte == 2 ? 3 : 4))))) {
					if ((uni != 0) || (nbyte != 2)) {
						// 2 bytes is acceptable for '\0'
						bad_utf("overlong sequence");
						uni = 0xFFFD;
					}
				}
				if ((uni == 0xFFFE) || (uni == 0xFFFF)) {
					bad_utf("illegal code");
				}
			}
			res.push_back(uni);
			break;
		default:
			res.push_back(static_cast<unsigned char>(first));
			break;
		}
	}
	return res;
}

} // namespace Unicode

} // namespace openmsx
