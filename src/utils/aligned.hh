#ifndef ALIGNED_HH
#define ALIGNED_HH

#include "inline.hh"

#include <cstdint>
#include <cstring>

// Only need to (more strictly) align when SSE is actually enabled.
#ifdef __SSE2__
inline constexpr size_t SSE_ALIGNMENT = 16;
#else
inline constexpr size_t SSE_ALIGNMENT = 0; // alignas(0) has no effect
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

#endif // ALIGNED_HH
