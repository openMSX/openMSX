#ifndef ENDIAN_HH
#define ENDIAN_HH

#include "inline.hh"
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>

namespace Endian {

inline constexpr bool BIG    = std::endian::native == std::endian::big;
inline constexpr bool LITTLE = std::endian::native == std::endian::little;
static_assert(BIG || LITTLE, "mixed endian not supported");

// Reverse bytes in a 16-bit number: 0x1234 becomes 0x3412
[[nodiscard]] static inline uint16_t byteswap16(uint16_t x)
{
	// This sequence generates 'optimal' code on a wide range of gcc/clang
	// versions (a single rotate instruction on x86). The newer compiler
	// versions also do 'the right thing' for the simpler expression below.
	// Those newer compilers also support __builtin_bswap16() but that
	// doesn't generate better code (and is less portable).
	return ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8);
	//return (x << 8) | (x >> 8);
}

// Reverse bytes in a 32-bit number: 0x12345678 becomes 0x78563412
[[nodiscard]] static inline uint32_t byteswap32(uint32_t x)
{
#if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 3))
	// Starting from gcc-4.3 there's a builtin function for this.
	// E.g. on x86 this is translated to a single 'bswap' instruction.
	return __builtin_bswap32(x);
#else
	return  (x << 24)               |
	       ((x <<  8) & 0x00ff0000) |
	       ((x >>  8) & 0x0000ff00) |
	        (x >> 24);
#endif
}

// Reverse bytes in a 64-bit value: 0x1122334455667788 becomes 0x8877665544332211
[[nodiscard]] static inline uint64_t byteswap64(uint64_t x)
{
#if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 3))
	// Starting from gcc-4.3 there's a builtin function for this.
	// E.g. on x86 this is translated to a single 'bswap' instruction.
	return __builtin_bswap64(x);
#else
	return (uint64_t(byteswap32(x >>  0)) << 32) |
	       (uint64_t(byteswap32(x >> 32)) <<  0);
#endif
}

// Use overloading to get a (statically) polymorphic byteswap() function.
[[nodiscard]] static inline uint16_t byteswap(uint16_t x) { return byteswap16(x); }
[[nodiscard]] static inline uint32_t byteswap(uint32_t x) { return byteswap32(x); }
[[nodiscard]] static inline uint64_t byteswap(uint64_t x) { return byteswap64(x); }


// Identity operator, simply returns the given value.
struct Ident {
	[[nodiscard]] inline auto operator()(std::integral auto t) const { return t; }
};

// Byte-swap operator, swap bytes in the given value (16 or 32 bit).
struct ByteSwap {
	[[nodiscard]] inline auto operator()(std::integral auto t) const { return byteswap(t); }
};

// Helper class that stores a value and allows to read/write that value. Though
// right before it is loaded/stored the value is transformed by a configurable
// operation.
// TODO If needed this can be extended with stuff like operator+= ....
template<std::integral T, std::invocable<T> Op> class EndianT {
public:
	EndianT() = default; // leave uninitialized
	EndianT(T t_)                  { Op op; t = op(t_); }
	[[nodiscard]] inline operator T() const { Op op; return op(t); }
	inline EndianT& operator=(T a) { Op op; t = op(a); return *this; }
private:
	T t;
};

// Define the types B16, B32, L16, L32.
//
// Typically these types are used to define the layout of external structures
// For example:
//
//   struct FATDirectoryEntry {
//      char filename[8];
//      char extension[3];
//      ...
//      Endian::L32 size;  // 32-bit little endian value
//   };
//   ...
//   unsigned s = myDirEntry.size; // Possibly performs endianess conversion.
//   yourDirEntry.size = s;        // If native endianess is already correct
//                                 // this has no extra overhead.
//
// You can assign and read values in native endianess to values of these types.
// So basically in a single location define the structure with the correct
// endianess and in all other places use the value as-if it were a native type.
//
// Note that these types should still be correctly aligned (e.g. L32 should be
// 4-byte aligned). For unaligned access use the functions below.
//
template<bool> struct ConvBig;
template<> struct ConvBig   <true > : Ident {};
template<> struct ConvBig   <false> : ByteSwap {};
template<bool> struct ConvLittle;
template<> struct ConvLittle<true > : ByteSwap {};
template<> struct ConvLittle<false> : Ident {};
using B16 = EndianT<uint16_t, ConvBig   <BIG>>;
using L16 = EndianT<uint16_t, ConvLittle<BIG>>;
using B32 = EndianT<uint32_t, ConvBig   <BIG>>;
using L32 = EndianT<uint32_t, ConvLittle<BIG>>;
using B64 = EndianT<uint64_t, ConvBig   <BIG>>;
using L64 = EndianT<uint64_t, ConvLittle<BIG>>;
static_assert(sizeof(B16)  == 2, "must have size 2");
static_assert(sizeof(L16)  == 2, "must have size 2");
static_assert(sizeof(B32)  == 4, "must have size 4");
static_assert(sizeof(L32)  == 4, "must have size 4");
static_assert(sizeof(B64)  == 8, "must have size 8");
static_assert(sizeof(L64)  == 8, "must have size 8");
static_assert(alignof(B16) <= 2, "may have alignment 2");
static_assert(alignof(L16) <= 2, "may have alignment 2");
static_assert(alignof(B32) <= 4, "may have alignment 4");
static_assert(alignof(L32) <= 4, "may have alignment 4");
static_assert(alignof(B64) <= 8, "may have alignment 8");
static_assert(alignof(L64) <= 8, "may have alignment 8");


// Helper functions to read/write aligned 16/32 bit values.
inline void writeB16(void* p, uint16_t x)
{
	*reinterpret_cast<B16*>(p) = x;
}
inline void writeL16(void* p, uint16_t x)
{
	*reinterpret_cast<L16*>(p) = x;
}
inline void writeB32(void* p, uint32_t x)
{
	*reinterpret_cast<B32*>(p) = x;
}
inline void writeL32(void* p, uint32_t x)
{
	*reinterpret_cast<L32*>(p) = x;
}

[[nodiscard]] inline uint16_t readB16(const void* p)
{
	return *reinterpret_cast<const B16*>(p);
}
[[nodiscard]] inline uint16_t readL16(const void* p)
{
	return *reinterpret_cast<const L16*>(p);
}
[[nodiscard]] inline uint32_t readB32(const void* p)
{
	return *reinterpret_cast<const B32*>(p);
}
[[nodiscard]] inline uint32_t readL32(const void* p)
{
	return *reinterpret_cast<const L32*>(p);
}

// Read/write big/little 16/24/32/64-bit values to/from a (possibly) unaligned
// memory location. If the host architecture supports unaligned load/stores
// (e.g. x86), these functions perform a single load/store (with possibly an
// adjust operation on the value if the endianess is different from the host
// endianess). If the architecture does not support unaligned memory operations
// (e.g. early ARM architectures), the operation is split into byte accesses.

template<bool SWAP> static ALWAYS_INLINE void write_UA(void* p, std::integral auto x)
{
	if constexpr (SWAP) x = byteswap(x);
	memcpy(p, &x, sizeof(x));
}
ALWAYS_INLINE void write_UA_B16(void* p, uint16_t x)
{
	write_UA<LITTLE>(p, x);
}
ALWAYS_INLINE void write_UA_L16(void* p, uint16_t x)
{
	write_UA<BIG>(p, x);
}
ALWAYS_INLINE void write_UA_L24(void* p, uint32_t x)
{
	assert(x < 0x1000000);
	auto* v = static_cast<uint8_t*>(p);
	v[0] = (x >>  0) & 0xff;
	v[1] = (x >>  8) & 0xff;
	v[2] = (x >> 16) & 0xff;
}
ALWAYS_INLINE void write_UA_B32(void* p, uint32_t x)
{
	write_UA<LITTLE>(p, x);
}
ALWAYS_INLINE void write_UA_L32(void* p, uint32_t x)
{
	write_UA<BIG>(p, x);
}
ALWAYS_INLINE void write_UA_B64(void* p, uint64_t x)
{
	write_UA<LITTLE>(p, x);
}
ALWAYS_INLINE void write_UA_L64(void* p, uint64_t x)
{
	write_UA<BIG>(p, x);
}

template<bool SWAP, std::integral T> [[nodiscard]] static ALWAYS_INLINE T read_UA(const void* p)
{
	T x;
	memcpy(&x, p, sizeof(x));
	if constexpr (SWAP) x = byteswap(x);
	return x;
}
[[nodiscard]] ALWAYS_INLINE uint16_t read_UA_B16(const void* p)
{
	return read_UA<LITTLE, uint16_t>(p);
}
[[nodiscard]] ALWAYS_INLINE uint16_t read_UA_L16(const void* p)
{
	return read_UA<BIG, uint16_t>(p);
}
[[nodiscard]] ALWAYS_INLINE uint32_t read_UA_L24(const void* p)
{
	const auto* v = static_cast<const uint8_t*>(p);
	return (v[0] << 0) | (v[1] << 8) | (v[2] << 16);
}
[[nodiscard]] ALWAYS_INLINE uint32_t read_UA_B32(const void* p)
{
	return read_UA<LITTLE, uint32_t>(p);
}
[[nodiscard]] ALWAYS_INLINE uint32_t read_UA_L32(const void* p)
{
	return read_UA<BIG, uint32_t>(p);
}
[[nodiscard]] ALWAYS_INLINE uint64_t read_UA_B64(const void* p)
{
	return read_UA<LITTLE, uint64_t>(p);
}
[[nodiscard]] ALWAYS_INLINE uint64_t read_UA_L64(const void* p)
{
	return read_UA<BIG, uint64_t>(p);
}


// Like the types above, but these don't need to be aligned.

class UA_B16 {
public:
	[[nodiscard]] inline operator uint16_t() const { return read_UA_B16(x); }
	inline UA_B16& operator=(uint16_t a) { write_UA_B16(x, a); return *this; }
private:
	uint8_t x[2];
};

class UA_L16 {
public:
	[[nodiscard]] inline operator uint16_t() const { return read_UA_L16(x); }
	inline UA_L16& operator=(uint16_t a) { write_UA_L16(x, a); return *this; }
private:
	uint8_t x[2];
};

class UA_L24 {
public:
	inline operator uint32_t() const     { return read_UA_L24(x); }
	inline UA_L24& operator=(uint32_t a) { write_UA_L24(x, a); return *this; }
private:
	uint8_t x[3];
};

class UA_B32 {
public:
	[[nodiscard]] inline operator uint32_t() const { return read_UA_B32(x); }
	inline UA_B32& operator=(uint32_t a) { write_UA_B32(x, a); return *this; }
private:
	uint8_t x[4];
};

class UA_L32 {
public:
	[[nodiscard]] inline operator uint32_t() const { return read_UA_L32(x); }
	inline UA_L32& operator=(uint32_t a) { write_UA_L32(x, a); return *this; }
private:
	uint8_t x[4];
};

static_assert(sizeof(UA_B16)  == 2, "must have size 2");
static_assert(sizeof(UA_L16)  == 2, "must have size 2");
static_assert(sizeof(UA_L24)  == 3, "must have size 3");
static_assert(sizeof(UA_B32)  == 4, "must have size 4");
static_assert(sizeof(UA_L32)  == 4, "must have size 4");
static_assert(alignof(UA_B16) == 1, "must have alignment 1");
static_assert(alignof(UA_L16) == 1, "must have alignment 1");
static_assert(alignof(UA_L24) == 1, "must have alignment 1");
static_assert(alignof(UA_B32) == 1, "must have alignment 1");
static_assert(alignof(UA_L32) == 1, "must have alignment 1");

// Template meta-programming.
// Get a type of the same size of the given type that stores the value in a
// specific endianess. Typically used in template functions that can work on
// either 16 or 32 bit values.
//  usage:
//    using LE_T = typename Endian::Little<T>::type;
//  The type LE_T is now a type that stores values of the same size as 'T'
//  in little endian format (independent of host endianess).
template<typename> struct Little;
template<> struct Little<uint8_t > { using type = uint8_t; };
template<> struct Little<uint16_t> { using type = L16; };
template<> struct Little<uint32_t> { using type = L32; };
template<typename> struct Big;
template<> struct Big<uint8_t > { using type = uint8_t; };
template<> struct Big<uint16_t> { using type = B16; };
template<> struct Big<uint32_t> { using type = B32; };

} // namespace Endian

#endif
