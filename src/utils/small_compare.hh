#ifndef SMALL_COMPARE_HH
#define SMALL_COMPARE_HH

// small_compare utility function
//
// This can be used to replace
//   string_ref s1 = ...
//   if (s1 == "foo") { ... }
// with
//   if (small_compare<'f','o','o'>(s1)) { ... }
// This looks more ugly, but it can execute (much) faster.
//
// The first variant internally calls memcmp() to do the actual string
// comparison (after the string length has been checked).
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
#include "build-info.hh"
#include "string_ref.hh"
#include "type_traits.hh"
#include <cstdint>
#include <cstring>

// Loader can load an 8/16/32/64 unaligned value.
struct Load8 {
	using type = uint8_t;
	type operator()(const void* p) { return *reinterpret_cast<const uint8_t*>(p); }
};
struct Load16 {
	using type = uint16_t;
	type operator()(const void* p) { return unalignedLoad16(p); }
};
struct Load32 {
	using type = uint32_t;
	type operator()(const void* p) { return unalignedLoad32(p); }
};
struct Load64 {
	using type = uint64_t;
	type operator()(const void* p) { return unalignedLoad64(p); }
};
struct ErrorNotSupportedLoader; // load only implemented up to 64 bit
template<size_t N> struct SelectLoader
	: if_c<N <= 1, Load8,
	  if_c<N <= 2, Load16,
	  if_c<N <= 4, Load32,
	  if_c<N <= 8, Load64,
	  ErrorNotSupportedLoader>>>> {};


// ScVal-little-endian
template<typename T, T v, T m, T s, char ...Ns> struct ScValLeImpl;
template<typename T, T v, T m, T s> struct ScValLeImpl<T, v, m, s> {
	static const T value = v;
	static const T mask  = m;
};
template<typename T, T v, T m, T s, char N0, char ...Ns> struct ScValLeImpl<T, v, m, s, N0, Ns...>
	: ScValLeImpl<T, v + (T(N0) << s), (m << 8) + 255, s + 8, Ns...> {};
template<typename T, char ...Ns> struct ScValLe : ScValLeImpl<T, 0, 0, 0, Ns...> {};

// ScVal-big-endian
template<typename T, T v, T m, char ...Ns> struct ScValBeImpl;
template<typename T, T v, T m> struct ScValBeImpl<T, v, m> {
	static const T value = v;
	static const T mask  = ~m;
};
template<typename T, T v, T m, char N0, char ...Ns> struct ScValBeImpl<T, v, m, N0, Ns...>
	: ScValBeImpl<T, (v << 8) + N0, (m >> 8), Ns...> {};
template<typename T, char ...Ns> struct ScValBe : ScValBeImpl<T, 0, -1, Ns...> {};

// ScVal: combines all given characters in one value of type T, also computes a
// mask-value with 1-bits in the 'used' positions.
template<typename T, char ...Ns> struct ScVal
	: if_c<openmsx::OPENMSX_BIGENDIAN, ScValBe<T, Ns...>, ScValLe<T, Ns...>> {};


template<char ...Ns> struct SmallCompare {
	using Loader = SelectLoader<sizeof...(Ns)>;
	using C = ScVal<typename Loader::type, Ns...>;
	// workaround gcc-4.7 bug
	//static const auto value = C::value;
	//static const auto mask  = C::mask;
	static const typename Loader::type value = C::value;
	static const typename Loader::type mask  = C::mask;
};

// The actual small-fixed-string-comparison.
template<char ...Ns> bool small_compare(const char* p)
{
	// workaround gcc-4.7 bug
	//using SC = SmallCompare<Ns...>;
	typedef SmallCompare<Ns...> SC;
	typename SC::Loader loader;
	return (loader(p) & SC::mask) == SC::value;
}

template<char ...Ns> bool small_compare(string_ref str)
{
	if (str.size() != sizeof...(Ns)) return false;
	return small_compare<Ns...>(str.data());
}

#endif
