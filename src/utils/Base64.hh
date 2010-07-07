// $Id$

#ifndef BASE64_HH
#define BASE64_HH

#include <string>

namespace Base64 {
	std::string encode(const void* input, int len);
	std::string decode(const std::string& input);
}

#endif
