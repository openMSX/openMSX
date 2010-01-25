// $Id$

#ifndef HEXDUMP_HH
#define HEXDUMP_HH

#include <string>

namespace HexDump {
	std::string encode(const void* input, int len, bool newlines = true);
	std::string decode(const std::string& input);
}

#endif
