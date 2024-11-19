#ifndef BASE64_HH
#define BASE64_HH

#include "MemBuffer.hh"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Base64 {
	[[nodiscard]] std::string encode(std::span<const uint8_t> input);
	[[nodiscard]] openmsx::MemBuffer<uint8_t> decode(std::string_view input);
	[[nodiscard]] bool decode_inplace(std::string_view input, std::span<uint8_t> output);
}

#endif
