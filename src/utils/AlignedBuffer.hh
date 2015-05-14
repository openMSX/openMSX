#ifndef ALIGNEDBUFFER_HH
#define ALIGNEDBUFFER_HH

#include "alignof.hh"
#include <type_traits>
#include <cassert>
#include <cstdint>
#include <cstddef>

namespace openmsx {

// Hack for visual studio, remove when we can use c++11 alignas/alignof
#ifdef _MSC_VER
  #ifdef _WIN64
    #define MAX_ALIGN_V 16
  #else
    #define MAX_ALIGN_V  8
  #endif
#else
  // This only works starting from gcc-4-9
  //  #define MAX_ALIGN_V ALIGNOF(MAX_ALIGN_T)
  #ifdef __x86_64
    #define MAX_ALIGN_V 16
  #else
    #define MAX_ALIGN_V  8
  #endif
#endif

// Interface for an aligned buffer.
// This type doesn't itself provide any storage, it only 'refers' to some
// storage. In that sense it is very similar in usage to a 'uint8_t*'.
//
// For example:
//   void f1(uint8_t* buf) {
//     ... uint8_t x = buf[7];
//     ... buf[4] = 10;
//     ... uint8_t* begin = buf;
//     ... uint8_t* end   = buf + 12;
//   }
//   void f2(AlignedBuffer& buf) {
//     // The exact same syntax as above, the only difference is that here
//     // the compiler knows at compile-time the alignment of 'buf', while
//     // above the compiler must assume worst case alignment.
//   }
#ifdef _MSC_VER
// TODO in the future use the c++11 'alignas' feature
__declspec (align(MAX_ALIGN_V))
#endif
class AlignedBuffer
{
public:
	static const size_t ALIGNMENT = MAX_ALIGN_V;

	operator       uint8_t*()       { return p(); }
	operator const uint8_t*() const { return p(); }

	      uint8_t* operator+(ptrdiff_t i)       { return p() + i; };
	const uint8_t* operator+(ptrdiff_t i) const { return p() + i; };

	      uint8_t& operator[](int           i)       { return *(p() + i); }
	const uint8_t& operator[](int           i) const { return *(p() + i); }
	      uint8_t& operator[](unsigned int  i)       { return *(p() + i); }
	const uint8_t& operator[](unsigned int  i) const { return *(p() + i); }
	      uint8_t& operator[](long          i)       { return *(p() + i); }
	const uint8_t& operator[](long          i) const { return *(p() + i); }
	      uint8_t& operator[](unsigned long i)       { return *(p() + i); }
	const uint8_t& operator[](unsigned long i) const { return *(p() + i); }

private:
	      uint8_t* p()       { return reinterpret_cast<      uint8_t*>(this); }
	const uint8_t* p() const { return reinterpret_cast<const uint8_t*>(this); }
}
#ifndef _MSC_VER
__attribute__((__aligned__((MAX_ALIGN_V))))
#endif
;
static_assert(ALIGNOF(AlignedBuffer) == AlignedBuffer::ALIGNMENT, "must be aligned");


// Provide actual storage for the AlignedBuffer
// A possible alternative is to use a union.
template<size_t N> class AlignedByteArray : public AlignedBuffer
{
public:
	size_t size() const { return N; }
	      uint8_t* data()       { return dat; }
	const uint8_t* data() const { return dat; }

private:
	uint8_t dat[N];

}
// Repeat alignment because Clang 3.2svn does not inherit it from an empty
// base class.
#ifndef _MSC_VER
__attribute__((__aligned__((MAX_ALIGN_V))))
#endif
;
static_assert(ALIGNOF(AlignedByteArray<13>) == AlignedBuffer::ALIGNMENT,
              "must be aligned");
static_assert(sizeof(AlignedByteArray<32>) == 32,
              "we rely on the empty-base optimization");


/** Cast one pointer type to another pointer type.
  * When asserts are enabled this checks whether the original pointer is
  * properly aligned to point to the destination type.
  */
template<typename T> static inline T aligned_cast(void* p)
{
	static_assert(std::is_pointer<T>::value,
	              "can only perform aligned_cast on pointers");
	assert((reinterpret_cast<uintptr_t>(p) %
	        ALIGNOF(typename std::remove_pointer<T>::type)) == 0);
	return reinterpret_cast<T>(p);
}

} //namespace openmsx

#endif
