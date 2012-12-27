// $Id$

#ifndef BASE64_HH
#define BASE64_HH

#include <string>

namespace Base64 {
	std::string encode(const void* input, size_t len);
	std::string decode(const std::string& input);
}

#endif
