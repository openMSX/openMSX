// $Id: $

#ifndef BASE64_HH
#define BASE64_HH

#include <string>

namespace Base64 {
	std::string encode(const void* input, int len, bool newlines = true);
	std::string decode(const std::string& input);
}

#endif
