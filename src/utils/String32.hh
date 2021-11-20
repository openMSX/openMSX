#ifndef STRING32_HH
#define STRING32_HH

#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>

// Given a buffer of max size 4GB, 32 bits are enough to address each position
// in that buffer.
// - On 32-bit systems we can use a pointer to a location in the buffer.
// - On 64-bit systems a pointer is too large (64 bit), but an index in the
//   buffer works fine. (An index works fine on 32-bit systems as well, but is
//   slightly less efficient).
// The String32 helper abstracts the difference between the two above
// approaches. In both cases it is a 32-bit type (hence the name). And on
// both 32/64-bit systems it uses the more efficient implementation.

using String32 = std::conditional_t<
	(sizeof(char*) > sizeof(uint32_t)), // is a pointer bigger than an uint32_t
	uint32_t,                           // yes -> use uint32_t
	const char*>;                       // no  -> directly use pointer

// convert string in buffer to String32
constexpr void toString32(const char* buffer, const char* str, uint32_t& result) {
	assert(buffer <= str);
	assert(str < (buffer + std::numeric_limits<uint32_t>::max()));
	result = str - buffer;
}
constexpr void toString32(const char* /*buffer*/, const char* str, const char*& result) {
	result = str;
}

// convert String32 back to string in buffer
[[nodiscard]] constexpr const char* fromString32(const char* buffer, uint32_t str32) {
	return buffer + str32;
}
[[nodiscard]] constexpr const char* fromString32(const char* /*buffer*/, const char* str32) {
	return str32;
}

#endif
