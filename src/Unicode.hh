// $Id$

#ifndef UNICODE_HH
#define UNICODE_HH

#include <string>

namespace openmsx {

namespace Unicode
{
	//decodes a a string possibly containing UTF-8 sequences to a 
	//string of 8-bit characters.
	// characters >= 0x100 are mapped to '?' for now
	std::string utf8ToAscii(const std::string& utf8);
}

} // namespace openmsx

#endif
