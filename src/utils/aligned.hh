#ifndef ALIGNED_HH
#define ALIGNED_HH

#include "build-info.hh"
#include <stdint.h>
#include <cassert>
#include <cstring>

#ifdef _MSC_VER
#define ALIGNED(EXPRESSION, ALIGNMENT) __declspec (align(ALIGNMENT)) EXPRESSION
#else // GCC style
#define ALIGNED(EXPRESSION, ALIGNMENT) EXPRESSION __attribute__((__aligned__((ALIGNMENT))));
#endif

// Only need to (more strictly) align when SSE is actually enabled.
#ifdef __SSE2__
#define SSE_ALIGNED(EXPRESSION) ALIGNED(EXPRESSION, 16)
#else
#define SSE_ALIGNED(EXPRESSION) EXPRESSION
#endif


// Unaligned loads and stores.
template<typename T> static inline T unalignedLoad(const void* p)
{
	if (openmsx::OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		return *reinterpret_cast<const T*>(p);
	} else {
		T t;
		memcpy(&t, p, sizeof(t));
		return t;
	}
}
template<typename T> static inline void unalignedStore(void* p, T t)
{
	if (openmsx::OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		*reinterpret_cast<T*>(p) = t;
	} else {
		memcpy(p, &t, sizeof(t));
	}
}

static inline uint16_t unalignedLoad16(const void* p) {
	return unalignedLoad<uint16_t>(p);
}
static inline uint32_t unalignedLoad32(const void* p) {
	return unalignedLoad<uint32_t>(p);
}
static inline uint64_t unalignedLoad64(const void* p) {
	return unalignedLoad<uint64_t>(p);
}
static inline void unalignedStore16(void* p, uint16_t v) {
	unalignedStore(p, v);
}
static inline void unalignedStore32(void* p, uint32_t v) {
	unalignedStore(p, v);
}
static inline void unalignedStore64(void* p, uint64_t v) {
	unalignedStore(p, v);
}


// assume_aligned, assume_SSE_aligned
//
// Let the compiler know a pointer is more strictly aligned than guaranteed by
// the C++ language rules. Sometimes this allows the compiler to generate more
// efficient code (typically when auto-vectorization is performed).
//
// Example:
//   int* p = ...
//   assume_SSE_aligned(p); // compiler can now assume 16-byte alignment for p

// clang offers __has_builtin, fallback for other compilers
#ifndef __has_builtin
#  define __has_builtin(x) 0
#endif

// gcc-version check macro
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#  define GNUC_PREREQ(maj, min) \
    (((maj) * 100 + (min)) <= (__GNUC__ * 100 + __GNUC_MINOR__))
#else
#  define GNUC_PREREQ(maj, min) 0
#endif

template<size_t A, typename T>
static inline void assume_aligned(T* __restrict & ptr)
{
#ifdef DEBUG // only check in debug build
	assert((reinterpret_cast<uintptr_t>(ptr) % A) == 0);
#endif

#if __has_builtin(__builtin_assume_aligned) || GNUC_PREREQ(4, 7)
	ptr = static_cast<T* __restrict>(__builtin_assume_aligned(ptr, A));
#else
	(void)ptr; // nothing
#endif
}

template<typename T> static inline void assume_SSE_aligned(
#ifdef __SSE2__
		T* __restrict & ptr) { assume_aligned<16>(ptr); }
#else
		T* __restrict & /*ptr*/) { /* nothing */ }
#endif

#endif // ALIGNED_HH
