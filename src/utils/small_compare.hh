#ifndef SMALL_COMPARE_HH
#define SMALL_COMPARE_HH

// small_compare utility function
//
// This can be used to replace
//   string_view s1 = ...
//   if (s1 == "foo") { ... }
// with
//   if (small_compare<"foo">(s1)) { ... }
// This looks more ugly, but it can execute faster.
//
// The first variant internally calls memcmp() to do the actual string
// comparison (after the string length has been checked).
// Update: gcc-12 optimizes memcmp() on small string-literals better:
//         e.g. it performs a 7-char comparison by using 2 4-byte loads
//              where the middle byte overlaps (older versions did a 4-byte,
//              2-byte and 1-byte load)
//         In the future, when gcc-12 is more common, we could remove this
//         small_compare utility.
//
// The second variant does a 4 byte unaligned load, masks the unnecessary 4th
// byte (so and with 0x00ffffff) and then compares with 0x006f6f66 ('f'->0x66,
// 'o'->0x6f) (on big endian system the mask and comparison constants are
// different). So only 3 instructions instead of a call to memcmp().
//
// In the general case small_compare does a 1, 2, 4 or 8 byte unaligned load
// (only strings upto 8 characters are supported). Possibly followed by a 'and'
// instruction (only needed for lengths 3, 5, 6 or 7), followed by a compare
// with a constant. This is much faster than memcmp().
//
// There are also some disadvantages.
// - (minor) We need unaligned load instructions. Many CPU architectures
//   support these, but even in case these must be emulated it's still fast.
// - (major) We read upto 8 bytes, and that might be past the end of the
//   input string. So it's only safe to use small_compare() if there's enough
//   padding at the end of the buffer.

#include "aligned.hh"
#include "endian.hh"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

// A wrapper around a (zero-terminated) string literal.
// Could be moved to a separate header (when also needed in other contexts).
template<size_t N> struct StringLiteral {
	constexpr StringLiteral(const char (&str)[N])
	{
		assert(str[N - 1] == '\0');
		std::copy_n(str, N - 1, value.data());
	}

	[[nodiscard]] constexpr size_t size() const { return value.size(); }
	[[nodiscard]] constexpr const char* data() const { return value.data(); }

	std::array<char, N - 1> value;
};

namespace detail {

// Loader can load an 8/16/32/64 unaligned value.
struct Load8 {
	using type = uint8_t;
	[[nodiscard]] static type operator()(const void* p) { return *std::bit_cast<const uint8_t*>(p); }
};
struct Load16 {
	using type = uint16_t;
	[[nodiscard]] static type operator()(const void* p) { return unalignedLoad16(p); }
};
struct Load32 {
	using type = uint32_t;
	[[nodiscard]] static type operator()(const void* p) { return unalignedLoad32(p); }
};
struct Load64 {
	using type = uint64_t;
	[[nodiscard]] static type operator()(const void* p) { return unalignedLoad64(p); }
};
struct ErrorNotSupportedLoader; // load only implemented up to 64 bit
template<size_t N> struct SelectLoader
	: std::conditional_t<N <= 1, Load8,
	  std::conditional_t<N <= 2, Load16,
	  std::conditional_t<N <= 4, Load32,
	  std::conditional_t<N <= 8, Load64,
	  ErrorNotSupportedLoader>>>> {};


template<typename T> constexpr std::pair<T, T> calcValueMask(auto str)
{
	T v = 0;
	T m = 0;
	int s = Endian::LITTLE ? 0 : (8 * (sizeof(T) - 1));
	for (size_t i = 0; i < str.size(); ++i) {
		v = T(v + (T(str.data()[i]) << s));
		m = T(m + (T(255) << s));
		s += Endian::LITTLE ? 8 : -8;
	}
	return {v, m};
}

} // namespace detail

template<StringLiteral Literal>
[[nodiscard]] bool small_compare(const char* p)
{
	using Loader = detail::SelectLoader<Literal.size()>;
	using Type = typename Loader::type;
	static constexpr auto vm = detail::calcValueMask<Type>(Literal);
	auto [value, mask] = vm;

	Loader loader;
	return (loader(p) & mask) == value;
}

template<StringLiteral Literal>
[[nodiscard]] bool small_compare(std::string_view str)
{
	if (str.size() != Literal.size()) return false;
	return small_compare<Literal>(str.data());
}

#endif
