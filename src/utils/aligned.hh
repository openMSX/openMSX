// $Id$

#ifndef ALIGNED_HH
#define ALIGNED_HH

#include "build-info.hh"
#include <stdint.h>

#ifdef _MSC_VER
#define ALIGNED(EXPRESSION, ALIGNMENT) __declspec (align(ALIGNMENT)) EXPRESSION
#else // GCC style
#define ALIGNED(EXPRESSION, ALIGNMENT) EXPRESSION __attribute__((__aligned__((ALIGNMENT))));
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

static inline uint32_t unalignedLoad32(const void* p) {
	return unalignedLoad<uint32_t>(p);
}
static inline uint64_t unalignedLoad64(const void* p) {
	return unalignedLoad<uint64_t>(p);
}
static inline void unalignedStore32(void* p, uint32_t v) {
	unalignedStore(p, v);
}
static inline void unalignedStore64(void* p, uint64_t v) {
	unalignedStore(p, v);
}

#endif // ALIGNED_HH
