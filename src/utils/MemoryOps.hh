#ifndef MEMORYOPS_HH
#define MEMORYOPS_HH

#include <cstddef>
#include <cstdint>
#include <span>

namespace openmsx::MemoryOps {

	void fill_2(std::span<uint32_t> out, uint32_t val0, uint32_t val1);

	[[nodiscard]] void* mallocAligned(size_t alignment, size_t size);
	void freeAligned(void* aligned);

} // namespace openmsx::MemoryOps

#endif
