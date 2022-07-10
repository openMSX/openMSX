#ifndef ALIGNED_HH
#define ALIGNED_HH

#include "inline.hh"
#include <cstdint>
#include <cassert>
#include <cstring>

// Only need to (more strictly) align when SSE is actually enabled.
#ifdef __SSE2__
constexpr size_t SSE_ALIGNMENT = 16;
#else
constexpr size_t SSE_ALIGNMENT = 0; // alignas(0) has no effect
#endif

// 'alignas(0)' has no effect according to the C++ standard, however gcc
// produces a warning for it. This for example triggers when using
//     alignas(SSE_ALIGNMENT)
// and the target has no SSE instructions (thus SSE_ALIGNMENT == 0).
// The following macro works around that.
#ifdef __SSE2__
#define ALIGNAS_SSE alignas(SSE_ALIGNMENT)
#else
#define ALIGNAS_SSE /*nothing*/
#endif

// Unaligned loads and stores.
template<typename T> [[nodiscard]] static ALWAYS_INLINE T unalignedLoad(const void* p)
{
	T t;
	memcpy(&t, p, sizeof(t));
	return t;
}
static ALWAYS_INLINE void unalignedStore(void* p, auto t)
{
	memcpy(p, &t, sizeof(t));
}

[[nodiscard]] ALWAYS_INLINE uint16_t unalignedLoad16(const void* p) {
	return unalignedLoad<uint16_t>(p);
}
[[nodiscard]] ALWAYS_INLINE uint32_t unalignedLoad32(const void* p) {
	return unalignedLoad<uint32_t>(p);
}
[[nodiscard]] ALWAYS_INLINE uint64_t unalignedLoad64(const void* p) {
	return unalignedLoad<uint64_t>(p);
}
ALWAYS_INLINE void unalignedStore16(void* p, uint16_t v) {
	unalignedStore(p, v);
}
ALWAYS_INLINE void unalignedStore32(void* p, uint32_t v) {
	unalignedStore(p, v);
}
ALWAYS_INLINE void unalignedStore64(void* p, uint64_t v) {
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
static ALWAYS_INLINE void assume_aligned(T* __restrict & ptr)
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

template<typename T> static ALWAYS_INLINE void assume_SSE_aligned(
#ifdef __SSE2__
		T* __restrict & ptr) { assume_aligned<16>(ptr); }
#else
		T* __restrict & /*ptr*/) { /* nothing */ }
#endif

#endif // ALIGNED_HH
