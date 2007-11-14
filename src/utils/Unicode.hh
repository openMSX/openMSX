// $Id$

#ifndef UNICODE_HH
#define UNICODE_HH

#include <string>

namespace openmsx {

namespace Unicode
{

typedef unsigned short unicode1_char; // Unicode 1 is 16 bit unsigned
typedef unsigned       unicode2_char; // Unicode 2 is 32 bit unsigned

typedef std::basic_string<unicode1_char> unicode1_string;
typedef std::basic_string<unicode2_char> unicode2_string;

	//decodes a a string possibly containing UTF-8 sequences to a
	//string of 8-bit characters.
	// characters >= 0x100 are mapped to '?' for now
	std::string utf8ToAscii(const std::string& utf8);
	//decodes a a string possibly containing UTF-8 sequences to a
	//string of unicode1 characters (unicode 2 is not yet supported)
	unicode1_string utf8ToUnicode1(const std::string& utf8);
}

} // namespace openmsx

#endif
