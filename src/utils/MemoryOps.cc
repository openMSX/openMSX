#include "MemoryOps.hh"

#include "build-info.hh"
#include "systemfuncs.hh"

#include "endian.hh"
#include "narrow.hh"
#include "stl.hh"
#include "unreachable.hh"

#include <bit>
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

namespace openmsx::MemoryOps {

#ifdef __SSE2__
#if ASM_X86_32 && defined _MSC_VER
// Gcc has the _mm_set1_epi64x() function for both 32 and 64 bit. Visual studio
// only has it for 64 bit. So we add it ourselves for vc++/32-bit. An
// alternative would be to always use this routine, but this generates worse
// code than the real _mm_set1_epi64x() function for gcc (both 32 and 64 bit).
[[nodiscard]] static inline __m128i _mm_set1_epi64x(uint64_t val)
{
	uint32_t low  = val >> 32;
	uint32_t high = val >>  0;
	return _mm_set_epi32(low, high, low, high);
}
#endif

static inline void memset_64_SSE(
	uint64_t* out, size_t num64, uint64_t val64)
{
	if (num64 == 0) [[unlikely]] return;

	// Align at 16-byte boundary.
	if (size_t(out) & 8) [[unlikely]] {
		out[0] = val64;
		++out; --num64;
	}

	__m128i val128 = _mm_set1_epi64x(narrow_cast<int64_t>(val64));
	const uint64_t* e = out + num64 - 3;
	for (/**/; out < e; out += 4) {
		_mm_storeu_si128(std::bit_cast<__m128i*>(out + 0), val128);
		_mm_storeu_si128(std::bit_cast<__m128i*>(out + 2), val128);
	}
	if (num64 & 2) [[unlikely]] {
		_mm_storeu_si128(std::bit_cast<__m128i*>(out), val128);
		out += 2;
	}
	if (num64 & 1) [[unlikely]] {
		out[0] = val64;
	}
}
#endif

static inline void memset_64(
	uint64_t* out, size_t num64, uint64_t val64)
{
	assert((size_t(out) % 8) == 0); // must be 8-byte aligned

#ifdef __SSE2__
	memset_64_SSE(out, num64, val64);
	return;
#endif
	const uint64_t* e = out + num64 - 3;
	for (/**/; out < e; out += 4) {
		out[0] = val64;
		out[1] = val64;
		out[2] = val64;
		out[3] = val64;
	}
	if (num64 & 2) [[unlikely]] {
		out[0] = val64;
		out[1] = val64;
		out += 2;
	}
	if (num64 & 1) [[unlikely]] {
		out[0] = val64;
	}
}

static inline void memset_32_2(
	uint32_t* out, size_t num32, uint32_t val0, uint32_t val1)
{
	assert((size_t(out) % 4) == 0); // must be 4-byte aligned
	if (num32 == 0) [[unlikely]] return;

	// Align at 8-byte boundary.
	if (size_t(out) & 4) [[unlikely]] {
		out[0] = val1; // start at odd pixel
		++out; --num32;
	}

	uint64_t val64 = Endian::BIG ? (uint64_t(val0) << 32) | val1
	                             : val0 | (uint64_t(val1) << 32);
	memset_64(std::bit_cast<uint64_t*>(out), num32 / 2, val64);

	if (num32 & 1) [[unlikely]] {
		out[num32 - 1] = val0;
	}
}

static inline void memset_32(uint32_t* out, size_t num32, uint32_t val32)
{
	assert((size_t(out) % 4) == 0); // must be 4-byte aligned

#if ASM_X86
#if defined _MSC_VER
	// VC++'s __stosd intrinsic results in emulator benchmarks
	// running about 7% faster than with memset_32_2, streaming or not,
	// and about 3% faster than the C code below.
	__stosd(std::bit_cast<unsigned long*>(out), val32, num32);
#else
	memset_32_2(out, num32, val32, val32);
#endif
#else
	uint32_t* e = out + num32 - 7;
	for (/**/; out < e; out += 8) {
		out[0] = val32;
		out[1] = val32;
		out[2] = val32;
		out[3] = val32;
		out[4] = val32;
		out[5] = val32;
		out[6] = val32;
		out[7] = val32;
	}
	if (num32 & 4) [[unlikely]] {
		out[0] = val32;
		out[1] = val32;
		out[2] = val32;
		out[3] = val32;
		out += 4;
	}
	if (num32 & 2) [[unlikely]] {
		out[0] = val32;
		out[1] = val32;
		out += 2;
	}
	if (num32 & 1) [[unlikely]] {
		out[0] = val32;
	}
#endif
}

template<typename Pixel> void MemSet<Pixel>::operator()(
	std::span<Pixel> out, Pixel val) const
{
	if constexpr (sizeof(Pixel) == 4) {
		memset_32(std::bit_cast<uint32_t*>(out.data()), out.size(), val);
	} else {
		UNREACHABLE;
	}
}

template<typename Pixel> void MemSet2<Pixel>::operator()(
	std::span<Pixel> out, Pixel val0, Pixel val1) const
{
	if constexpr (sizeof(Pixel) == 4) {
		memset_32_2(std::bit_cast<uint32_t*>(out.data()), out.size(), val0, val1);
	} else {
		UNREACHABLE;
	}
}

// Force template instantiation
template struct MemSet <uint32_t>;
template struct MemSet2<uint32_t>;



/** Aligned memory (de)allocation
 */

// Helper class to keep track of aligned/unaligned pointer pairs
class AllocMap
{
public:
	AllocMap(const AllocMap&) = delete;
	AllocMap(AllocMap&&) = delete;
	AllocMap& operator=(const AllocMap&) = delete;
	AllocMap& operator=(AllocMap&&) = delete;

	static AllocMap& instance() {
		static AllocMap oneInstance;
		return oneInstance;
	}

	void insert(void* aligned, void* unaligned) {
		if (!aligned) return;
		assert(!contains(allocMap, aligned, &Entry::aligned));
		allocMap.emplace_back(Entry{aligned, unaligned});
	}

	void* remove(void* aligned) {
		if (!aligned) return nullptr;
		// LIFO order is more likely than FIFO -> search backwards
		auto it = rfind_unguarded(allocMap, aligned, &Entry::aligned);
		// return the associated unaligned value
		void* unaligned = it->unaligned;
		move_pop_back(allocMap, it);
		return unaligned;
	}

private:
	AllocMap() = default;
	~AllocMap() {
		assert(allocMap.empty());
	}

	// typically contains 5-10 items, so (unsorted) vector is fine
	struct Entry {
		void* aligned;
		void* unaligned;
	};
	std::vector<Entry> allocMap;
};

void* mallocAligned(size_t alignment, size_t size)
{
	assert("must be a power of 2" && std::has_single_bit(alignment));
	assert(alignment >= sizeof(void*));
#if HAVE_POSIX_MEMALIGN
	void* aligned = nullptr;
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
	auto aligned = std::bit_cast<void*>(
		(std::bit_cast<uintptr_t>(unaligned) + t) & ~t);
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

} // namespace openmsx::MemoryOps
