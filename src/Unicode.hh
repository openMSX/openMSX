// $Id$

#ifndef __UNICODE_HH__
#define __UNICODE_HH__

#include <string>
#include <sstream>
#include <strings.h>

namespace openmsx {

class Unicode
{
public:
	//decodes a a string possibly containing UTF-8 sequences to a 
	//string of 8-bit characters.
	// characters >= 0x100 are mapped to '?' for now
	static std::string utf8ToAscii(const std::string& utf8);
};

}
#endif
