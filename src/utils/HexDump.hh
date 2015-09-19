#ifndef HEXDUMP_HH
#define HEXDUMP_HH

#include "MemBuffer.hh"
#include "string_ref.hh"
#include <string>
#include <cstdint>
#include <utility>

namespace HexDump {
	std::string encode(const uint8_t* input, size_t len, bool newlines = true);
	std::pair<openmsx::MemBuffer<uint8_t>, size_t> decode(string_ref input);
	bool decode_inplace(string_ref input, uint8_t* output, size_t outSize);
}

#endif
