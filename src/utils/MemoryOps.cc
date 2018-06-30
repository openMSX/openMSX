#include "MemoryOps.hh"
#include "likely.hh"
#include "build-info.hh"
#include "systemfuncs.hh"
#include "Math.hh"
#include "stl.hh"
#include "unreachable.hh"
#include <utility>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <new> // for std::bad_alloc
#if ASM_X86 && defined _MSC_VER
#include <intrin.h>	// for __stosd intrinsic
#endif
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx {
namespace MemoryOps {

#ifdef __SSE2__
#if ASM_X86_32 && defined _MSC_VER
// Gcc has the _mm_set1_epi64x() function for both 32 and 64 bit. Visual studio
// only has it for 64 bit. So we add it ourselves for vc++/32-bit. An
// alternative would be to always use this routine, but this generates worse
// code than the real _mm_set1_epi64x() function for gcc (both 32 and 64 bit).
static inline __m128i _mm_set1_epi64x(uint64_t val)
{
	uint32_t low  = val >> 32;
	uint32_t high = val >>  0;
	return _mm_set_epi32(low, high, low, high);
}
#endif

static inline void memset_64_SSE(
	uint64_t* dest, size_t num64, uint64_t val64)
{
	if (unlikely(num64 == 0)) return;

	// Align at 16-byte boundary.
	if (unlikely(size_t(dest) & 8)) {
		dest[0] = val64;
		++dest; --num64;
	}

	__m128i val128 = _mm_set1_epi64x(val64);
	uint64_t* e = dest + num64 - 3;
	for (/**/; dest < e; dest += 4) {
		_mm_store_si128(reinterpret_cast<__m128i*>(dest + 0), val128);
		_mm_store_si128(reinterpret_cast<__m128i*>(dest + 2), val128);
	}
	if (unlikely(num64 & 2)) {
		_mm_store_si128(reinterpret_cast<__m128i*>(dest), val128);
		dest += 2;
	}
	if (unlikely(num64 & 1)) {
		dest[0] = val64;
	}
}
#endif

static inline void memset_64(
        uint64_t* dest, size_t num64, uint64_t val64)
{
	assert((size_t(dest) % 8) == 0); // must be 8-byte aligned

#ifdef __SSE2__
	memset_64_SSE(dest, num64, val64);
	return;
#endif
	uint64_t* e = dest + num64 - 3;
	for (/**/; dest < e; dest += 4) {
		dest[0] = val64;
		dest[1] = val64;
		dest[2] = val64;
		dest[3] = val64;
	}
	if (unlikely(num64 & 2)) {
		dest[0] = val64;
		dest[1] = val64;
		dest += 2;
	}
	if (unlikely(num64 & 1)) {
		dest[0] = val64;
	}
}

static inline void memset_32_2(
	uint32_t* dest, size_t num32, uint32_t val0, uint32_t val1)
{
	assert((size_t(dest) % 4) == 0); // must be 4-byte aligned
	if (unlikely(num32 == 0)) return;

	// Align at 8-byte boundary.
	if (unlikely(size_t(dest) & 4)) {
		dest[0] = val1; // start at odd pixel
		++dest; --num32;
	}

	uint64_t val64 = OPENMSX_BIGENDIAN ? (uint64_t(val0) << 32) | val1
	                                   : val0 | (uint64_t(val1) << 32);
	memset_64(reinterpret_cast<uint64_t*>(dest), num32 / 2, val64);

	if (unlikely(num32 & 1)) {
		dest[num32 - 1] = val0;
	}
}

static inline void memset_32(uint32_t* dest, size_t num32, uint32_t val32)
{
	assert((size_t(dest) % 4) == 0); // must be 4-byte aligned

#if ASM_X86
#if defined _MSC_VER
	// VC++'s __stosd intrinsic results in emulator benchmarks
	// running about 7% faster than with memset_32_2, streaming or not,
	// and about 3% faster than the C code below.
	__stosd(reinterpret_cast<unsigned long*>(dest), val32, num32);
#else
	memset_32_2(dest, num32, val32, val32);
#endif
#elif defined __arm__
	// Ideally the first mov(*) instruction could be omitted (and then
	// replace 'r3' with '%[val]'. But this can cause problems in the
	// 'stm' instructions when the compiler chooses a register
	// 'bigger' than r4 for [val]. See commit message for LOTS more
	// details.
	asm volatile (
		"mov     r3, %[val]\n\t"  // (*) should not be needed
		"mov     r4, r3\n\t"
		"mov     r5, r3\n\t"
		"mov     r6, r3\n\t"
		"subs    %[num],%[num],#8\n\t"
		"bmi     1f\n"
		"mov     r8, r3\n\t"
		"mov     r9, r3\n\t"
		"mov     r10,r3\n\t"
		"mov     r12,r3\n\t"
	"0:\n\t"
		"stmia   %[dest]!,{r3,r4,r5,r6,r8,r9,r10,r12}\n\t"
		"subs    %[num],%[num],#8\n\t"
		"bpl     0b\n\t"
	"1:\n\t"
		"tst     %[num],#4\n\t"
		"it      ne\n\t"
		"stmne   %[dest]!,{r3,r4,r5,r6}\n\t"
		"tst     %[num],#2\n\t"
		"it      ne\n\t"
		"stmne   %[dest]!,{r3,r4}\n\t"
		"tst     %[num],#1\n\t"
		"it      ne\n\t"
		"strne   r3,[%[dest]]\n\t"

		: [dest] "=r"    (dest)
		, [num] "=r"     (num32)
		:       "[dest]" (dest)
		,       "[num]"  (num32)
		, [val] "r"      (val32)
		: "memory"
		, "r3","r4","r5","r6","r8","r9","r10","r12"
	);
	return;
#else
	uint32_t* e = dest + num32 - 7;
	for (/**/; dest < e; dest += 8) {
		dest[0] = val32;
		dest[1] = val32;
		dest[2] = val32;
		dest[3] = val32;
		dest[4] = val32;
		dest[5] = val32;
		dest[6] = val32;
		dest[7] = val32;
	}
	if (unlikely(num32 & 4)) {
		dest[0] = val32;
		dest[1] = val32;
		dest[2] = val32;
		dest[3] = val32;
		dest += 4;
	}
	if (unlikely(num32 & 2)) {
		dest[0] = val32;
		dest[1] = val32;
		dest += 2;
	}
	if (unlikely(num32 & 1)) {
		dest[0] = val32;
	}
#endif
}

static inline void memset_16_2(
	uint16_t* dest, size_t num16, uint16_t val0, uint16_t val1)
{
	if (unlikely(num16 == 0)) return;

	// Align at 4-byte boundary.
	if (unlikely(size_t(dest) & 2)) {
		dest[0] = val1; // start at odd pixel
		++dest; --num16;
	}

	uint32_t val32 = OPENMSX_BIGENDIAN ? (uint32_t(val0) << 16) | val1
	                                   : val0 | (uint32_t(val1) << 16);
	memset_32(reinterpret_cast<uint32_t*>(dest), num16 / 2, val32);

	if (unlikely(num16 & 1)) {
		dest[num16 - 1] = val0;
	}
}

static inline void memset_16(uint16_t* dest, size_t num16, uint16_t val16)
{
	memset_16_2(dest, num16, val16, val16);
}

template<typename Pixel> void MemSet<Pixel>::operator()(
	Pixel* dest, size_t num, Pixel val) const
{
	if (sizeof(Pixel) == 2) {
		memset_16(reinterpret_cast<uint16_t*>(dest), num, val);
	} else if (sizeof(Pixel) == 4) {
		memset_32(reinterpret_cast<uint32_t*>(dest), num, val);
	} else {
		UNREACHABLE;
	}
}

template<typename Pixel> void MemSet2<Pixel>::operator()(
	Pixel* dest, size_t num, Pixel val0, Pixel val1) const
{
	if (sizeof(Pixel) == 2) {
		memset_16_2(reinterpret_cast<uint16_t*>(dest), num, val0, val1);
	} else if (sizeof(Pixel) == 4) {
		memset_32_2(reinterpret_cast<uint32_t*>(dest), num, val0, val1);
	} else {
		UNREACHABLE;
	}
}

// Force template instantiation
template struct MemSet <uint16_t>;
template struct MemSet <uint32_t>;
template struct MemSet2<uint16_t>;
template struct MemSet2<uint32_t>;



/** Aligned memory (de)allocation
 */

// Helper class to keep track of aligned/unaligned pointer pairs
class AllocMap
{
public:
	static AllocMap& instance() {
		static AllocMap oneInstance;
		return oneInstance;
	}

	void insert(void* aligned, void* unaligned) {
		if (!aligned) return;
		assert(none_of(begin(allocMap), end(allocMap),
		               EqualTupleValue<0>(aligned)));
		allocMap.emplace_back(aligned, unaligned);
	}

	void* remove(void* aligned) {
		if (!aligned) return nullptr;
		// LIFO order is more likely than FIFO -> search backwards
		auto it = rfind_if_unguarded(allocMap,
		               EqualTupleValue<0>(aligned));
		// return the associated unaligned value
		void* unaligned = it->second;
		move_pop_back(allocMap, it);
		return unaligned;
	}

private:
	AllocMap() = default;
	~AllocMap() {
		assert(allocMap.empty());
	}

	// typically contains 5-10 items, so (unsorted) vector is fine
	std::vector<std::pair<void*, void*>> allocMap;
};

void* mallocAligned(size_t alignment, size_t size)
{
	assert("must be a power of 2" && Math::isPowerOfTwo(alignment));
	assert(alignment >= sizeof(void*));
#if HAVE_POSIX_MEMALIGN
	void* aligned;
	if (posix_memalign(&aligned, alignment, size)) {
		throw std::bad_alloc();
	}
	#if defined DEBUG
	AllocMap::instance().insert(aligned, aligned);
	#endif
	return aligned;
#elif defined _MSC_VER
	void* result = _aligned_malloc(size, alignment);
	if (!result && size) throw std::bad_alloc();
	return result;
#else
	auto t = alignment - 1;
	void* unaligned = malloc(size + t);
	if (!unaligned) {
		throw std::bad_alloc();
	}
	auto aligned = reinterpret_cast<void*>(
		(reinterpret_cast<size_t>(unaligned) + t) & ~t);
	AllocMap::instance().insert(aligned, unaligned);
	return aligned;
#endif
}

void freeAligned(void* aligned)
{
#if HAVE_POSIX_MEMALIGN
	#if defined DEBUG
	AllocMap::instance().remove(aligned);
	#endif
	free(aligned);
#elif defined _MSC_VER
	return _aligned_free(aligned);
#else
	void* unaligned = AllocMap::instance().remove(aligned);
	free(unaligned);
#endif
}

} // namespace MemoryOps
} // namespace openmsx
