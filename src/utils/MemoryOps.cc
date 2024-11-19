#include "MemoryOps.hh"

#include "systemfuncs.hh"

#include "endian.hh"
#include "ranges.hh"
#include "stl.hh"

#include <bit>
#include <cassert>
#include <cstdlib>
#include <new> // for std::bad_alloc

namespace openmsx::MemoryOps {

void fill_2(std::span<uint32_t> out, uint32_t val0, uint32_t val1)
{
	if (out.empty()) [[unlikely]] return;

	// Align at 64-bit boundary.
	if (std::bit_cast<uintptr_t>(out.data()) & alignof(uint32_t)) [[unlikely]] {
		out.front() = val1; // start at odd pixel
		out = out.subspan(1);
	}

	// Main loop: (aligned) 64-bit fill.
	std::span<uint64_t> out64{std::bit_cast<uint64_t*>(out.data()), out.size() / 2};
	uint64_t val64 = Endian::BIG ? (uint64_t(val0) << 32) | val1
	                             : val0 | (uint64_t(val1) << 32);
	ranges::fill(out64, val64);

	// Single remaining pixel.
	if (out.size() & 1) [[unlikely]] {
		out.back() = val0;
	}
}


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
