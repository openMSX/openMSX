// $Id$
//
#ifndef ENDIAN_HH
#define ENDIAN_HH

#include "alignof.hh"
#include "static_assert.hh"
#include "build-info.hh"
#include <stdint.h>

namespace Endian {

// Revese bytes in a 16-bit number: 0x1234 becomes 0x3412
static inline uint16_t bswap16(uint16_t x)
{
#if (__GNUC__ >= 2) && ASM_X86
	// Swapping the upper and lower byte in a 16-bit value can be done
	// by rotating the value (left or right) over 8 positions.
	//
	// TODO Why doesn't gcc translate the C code below to a single rotate
	// instruction? Is it because gcc isn't smart enough or because of some
	// other reason? I checked the x86 instruction timings and on modern
	// x86 CPUs ROR is a very fast instruction (though not on P4). It also
	// results in shorter code (and for openMSX speed of this operation is
	// not critical).
	if (!__builtin_constant_p(x)) {
		// Only if the expression cannot be evaluated at compile-time
		// we want to emit a rotate asm instruction.
		asm ("rorw $8, %0"
		     : "=r" (x)
		     : "0"  (x)
		     : "cc");
		return x;
	}
#endif
	return (x << 8) | (x >> 8);
}

// Revese bytes in a 32-bit number: 0x12345678 becomes 0x78563412
static inline uint32_t bswap32(uint32_t x)
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

// Use overloading to get a (statically) polymorphic bswap() function.
static inline uint16_t bswap(uint16_t x) { return bswap16(x); }
static inline uint32_t bswap(uint32_t x) { return bswap32(x); }


// Identity operator, simply returns the given value.
struct Ident {
	template <typename T> inline T operator()(T t) const { return t; }
};

// Byte-swap operator, swap bytes in the given value (16 or 32 bit).
struct BSwap {
	template <typename T> inline T operator()(T t) const { return bswap(t); }
};

// Helper class that stores a value and allows to read/write that value. Though
// right before it is loaded/stored the value is transformed by a configurable
// operation.
// TODO If needed this can be extended with stuff like operator+= ....
template<typename T, typename Op> class EndianT {
public:
	// TODO These constructors are useful, but they prevent this type from
	//      being used in a union. This limitation is removed in C++11.
	//EndianT()                      { /* leave uninitialized */ }
	//EndianT(T t_)                  { Op op; t = op(t_); }
	inline operator T() const      { Op op; return op(t); }
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
template<> struct ConvBig   <false> : BSwap {};
template<bool> struct ConvLittle;
template<> struct ConvLittle<true > : BSwap {};
template<> struct ConvLittle<false> : Ident {};
typedef EndianT<uint16_t, ConvBig   <openmsx::OPENMSX_BIGENDIAN> > B16;
typedef EndianT<uint16_t, ConvLittle<openmsx::OPENMSX_BIGENDIAN> > L16;
typedef EndianT<uint32_t, ConvBig   <openmsx::OPENMSX_BIGENDIAN> > B32;
typedef EndianT<uint32_t, ConvLittle<openmsx::OPENMSX_BIGENDIAN> > L32;
STATIC_ASSERT(sizeof(B16) == 2);
STATIC_ASSERT(sizeof(L16) == 2);
STATIC_ASSERT(sizeof(B32) == 4);
STATIC_ASSERT(sizeof(L32) == 4);
STATIC_ASSERT(ALIGNOF(B16) == 2);
STATIC_ASSERT(ALIGNOF(L16) == 2);
STATIC_ASSERT(ALIGNOF(B32) == 4);
STATIC_ASSERT(ALIGNOF(L32) == 4);


// Helper functions to read/write aligned 16/32 bit values.
static inline void writeB16(void* p, uint16_t x)
{
	*reinterpret_cast<B16*>(p) = x;
}
static inline void writeL16(void* p, uint16_t x)
{
	*reinterpret_cast<L16*>(p) = x;
}
static inline void writeB32(void* p, uint32_t x)
{
	*reinterpret_cast<B32*>(p) = x;
}
static inline void writeL32(void* p, uint32_t x)
{
	*reinterpret_cast<L32*>(p) = x;
}

static inline uint16_t readB16(const void* p)
{
	return *reinterpret_cast<const B16*>(p);
}
static inline uint16_t readL16(const void* p)
{
	return *reinterpret_cast<const L16*>(p);
}
static inline uint32_t readB32(const void* p)
{
	return *reinterpret_cast<const B32*>(p);
}
static inline uint32_t readL32(const void* p)
{
	return *reinterpret_cast<const L32*>(p);
}

// Read/write big/little 16/32-bit values to/from a (possibly) unaligned memory
// location. If the host architecture supports unaligned load/stores (e.g.
// x86), these functions perform a single load/store (with possibly an adjust
// operation on the value if the endianess is different from the host
// endianess). If the architecture does not support unaligned memory operations
// (e.g. early ARM architectures), the operation is split into byte accesses.

static inline void write_UA_B16(void* p_, uint16_t x)
{
	if (openmsx::OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		writeB16(p_, x);
	} else {
		uint8_t* p = static_cast<uint8_t*>(p_);
		p[0] = (x >> 8) & 0xff;
		p[1] = (x >> 0) & 0xff;
	}
}
static inline void write_UA_L16(void* p_, uint16_t x)
{
	if (openmsx::OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		writeL16(p_, x);
	} else {
		uint8_t* p = static_cast<uint8_t*>(p_);
		p[0] = (x >> 0) & 0xff;
		p[1] = (x >> 8) & 0xff;
	}
}
static inline void write_UA_B32(void* p_, uint32_t x)
{
	if (openmsx::OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		writeB32(p_, x);
	} else {
		uint8_t* p = static_cast<uint8_t*>(p_);
		p[0] = (x >> 24) & 0xff;
		p[1] = (x >> 16) & 0xff;
		p[2] = (x >>  8) & 0xff;
		p[3] = (x >>  0) & 0xff;
	}
}
static inline void write_UA_L32(void* p_, uint32_t x)
{
	if (openmsx::OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		writeL32(p_, x);
	} else {
		uint8_t* p = static_cast<uint8_t*>(p_);
		p[0] = (x >>  0) & 0xff;
		p[1] = (x >>  8) & 0xff;
		p[2] = (x >> 16) & 0xff;
		p[3] = (x >> 24) & 0xff;
	}
}

static inline uint16_t read_UA_B16(const void* p_)
{
	if (openmsx::OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		return readB16(p_);
	} else {
		const uint8_t* p = static_cast<const uint8_t*>(p_);
		return (p[0] << 8) | p[1];
	}
}
static inline uint16_t read_UA_L16(const void* p_)
{
	if (openmsx::OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		return readL16(p_);
	} else {
		const uint8_t* p = static_cast<const uint8_t*>(p_);
		return (p[1] << 8) | p[0];
	}
}
static inline uint32_t read_UA_B32(const void* p_)
{
	if (openmsx::OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		return readB32(p_);
	} else {
		const uint8_t* p = static_cast<const uint8_t*>(p_);
		return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	}
}
static inline uint32_t read_UA_L32(const void* p_)
{
	if (openmsx::OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		return readL32(p_);
	} else {
		const uint8_t* p = static_cast<const uint8_t*>(p_);
		return (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
	}
}



// Like the types above, but these don't need to be aligned.

class UA_B16 {
public:
	inline operator uint16_t() const     { return read_UA_B16(x); }
	inline UA_B16& operator=(uint16_t a) { write_UA_B16(x, a); return *this; }
private:
	uint8_t x[2];
};

class UA_L16 {
public:
	inline operator uint16_t() const     { return read_UA_L16(x); }
	inline UA_L16& operator=(uint16_t a) { write_UA_L16(x, a); return *this; }
private:
	uint8_t x[2];
};

class UA_B32 {
public:
	inline operator uint32_t() const     { return read_UA_B32(x); }
	inline UA_B32& operator=(uint32_t a) { write_UA_B32(x, a); return *this; }
private:
	uint8_t x[4];
};

class UA_L32 {
public:
	inline operator uint32_t() const     { return read_UA_L32(x); }
	inline UA_L32& operator=(uint32_t a) { write_UA_L32(x, a); return *this; }
private:
	uint8_t x[4];
};

STATIC_ASSERT(sizeof(UA_B16) == 2);
STATIC_ASSERT(sizeof(UA_L16) == 2);
STATIC_ASSERT(sizeof(UA_B32) == 4);
STATIC_ASSERT(sizeof(UA_L32) == 4);
STATIC_ASSERT(ALIGNOF(UA_B16) == 1);
STATIC_ASSERT(ALIGNOF(UA_L16) == 1);
STATIC_ASSERT(ALIGNOF(UA_B32) == 1);
STATIC_ASSERT(ALIGNOF(UA_L32) == 1);

// Template meta-programming.
// Get a type of the same size of the given type that stores the value in a
// specific endianess. Typically used in template functions that can work on
// either 16 or 32 bit values.
//  usage:
//    typedef typename Endian::Little<T>::type LE_T;
//  The type LE_T is now a type that stores values of the same size as 'T'
//  in little endian format (independent of host endianess).
template<typename> struct Little;
template<> struct Little<uint8_t > { typedef uint8_t type; };
template<> struct Little<uint16_t> { typedef L16     type; };
template<> struct Little<uint32_t> { typedef L32     type; };
template<typename> struct Big;
template<> struct Big<uint8_t > { typedef uint8_t type; };
template<> struct Big<uint16_t> { typedef B16     type; };
template<> struct Big<uint32_t> { typedef B32     type; };

} // namespace Endian

#endif


#if 0

///////////////////////////////////////////
// Small functions to inspect code quality

using namespace Endian;

uint16_t testSwap16(uint16_t x) { return bswap16(x); }
uint16_t testSwap16()         { return bswap16(0x1234); }
uint32_t testSwap32(uint32_t x) { return bswap32(x); }
uint32_t testSwap32()         { return bswap32(0x12345678); }


union T16 {
	B16 be;
	L16 le;
};

void test1(T16& t, uint16_t x) { t.le = x; }
void test2(T16& t, uint16_t x) { t.be = x; }
uint16_t test3(T16& t) { return t.le; }
uint16_t test4(T16& t) { return t.be; }

void testA(uint16_t& s, uint16_t x) { write_UA_L16(&s, x); }
void testB(uint16_t& s, uint16_t x) { write_UA_B16(&s, x); }
uint16_t testC(uint16_t& s) { return read_UA_L16(&s); }
uint16_t testD(uint16_t& s) { return read_UA_B16(&s); }


union T32 {
	B32 be;
	L32 le;
};

void test1(T32& t, uint32_t x) { t.le = x; }
void test2(T32& t, uint32_t x) { t.be = x; }
uint32_t test3(T32& t) { return t.le; }
uint32_t test4(T32& t) { return t.be; }

void testA(uint32_t& s, uint32_t x) { write_UA_L32(&s, x); }
void testB(uint32_t& s, uint32_t x) { write_UA_B32(&s, x); }
uint32_t testC(uint32_t& s) { return read_UA_L32(&s); }
uint32_t testD(uint32_t& s) { return read_UA_B32(&s); }


///////////////////////////////
// Simple unit test

#include <cassert>

int main()
{
	T16 t16;
	assert(sizeof(t16) == 2);

	t16.le = 0x1234;
	assert(t16.le == 0x1234);
	assert(t16.be == 0x3412);
	assert(read_UA_L16(&t16) == 0x1234);
	assert(read_UA_B16(&t16) == 0x3412);

	t16.be = 0x1234;
	assert(t16.le == 0x3412);
	assert(t16.be == 0x1234);
	assert(read_UA_L16(&t16) == 0x3412);
	assert(read_UA_B16(&t16) == 0x1234);

	write_UA_L16(&t16, 0xaabb);
	assert(t16.le == 0xaabb);
	assert(t16.be == 0xbbaa);
	assert(read_UA_L16(&t16) == 0xaabb);
	assert(read_UA_B16(&t16) == 0xbbaa);

	write_UA_B16(&t16, 0xaabb);
	assert(t16.le == 0xbbaa);
	assert(t16.be == 0xaabb);
	assert(read_UA_L16(&t16) == 0xbbaa);
	assert(read_UA_B16(&t16) == 0xaabb);


	T32 t32;
	assert(sizeof(t32) == 4);

	t32.le = 0x12345678;
	assert(t32.le == 0x12345678);
	assert(t32.be == 0x78563412);
	assert(read_UA_L32(&t32) == 0x12345678);
	assert(read_UA_B32(&t32) == 0x78563412);

	t32.be = 0x12345678;
	assert(t32.le == 0x78563412);
	assert(t32.be == 0x12345678);
	assert(read_UA_L32(&t32) == 0x78563412);
	assert(read_UA_B32(&t32) == 0x12345678);

	write_UA_L32(&t32, 0xaabbccdd);
	assert(t32.le == 0xaabbccdd);
	assert(t32.be == 0xddccbbaa);
	assert(read_UA_L32(&t32) == 0xaabbccdd);
	assert(read_UA_B32(&t32) == 0xddccbbaa);

	write_UA_B32(&t32, 0xaabbccdd);
	assert(t32.le == 0xddccbbaa);
	assert(t32.be == 0xaabbccdd);
	assert(read_UA_L32(&t32) == 0xddccbbaa);
	assert(read_UA_B32(&t32) == 0xaabbccdd);
}

#endif
