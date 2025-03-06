#include "MemoryOps.hh"

#include "endian.hh"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdlib>
#include <new>

#ifdef _WIN32
    #include <malloc.h>  // _aligned_malloc, _aligned_free
#endif

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
	std::ranges::fill(out64, val64);

	// Single remaining pixel.
	if (out.size() & 1) [[unlikely]] {
		out.back() = val0;
	}
}


void* mallocAligned(size_t alignment, size_t size)
{
	assert("must be a power of 2" && std::has_single_bit(alignment));
	assert(alignment >= sizeof(void*));
#ifdef _WIN32
	void* result = _aligned_malloc(size, alignment);
#else
	// Ensure size is a multiple of alignment for std::aligned_alloc()
	size = (size + alignment - 1) & ~(alignment - 1);
	void* result = std::aligned_alloc(alignment, size);
#endif
	if (!result && size) throw std::bad_alloc();
	return result;
}

void freeAligned(void* aligned)
{
#ifdef _WIN32
	_aligned_free(aligned);
#else
	std::free(aligned);
#endif
}

} // namespace openmsx::MemoryOps
