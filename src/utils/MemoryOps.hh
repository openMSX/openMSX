#ifndef MEMORYOPS_HH
#define MEMORYOPS_HH

#include <cstddef>
#include <span>

namespace openmsx::MemoryOps {

	template<typename Pixel> struct MemSet {
		void operator()(std::span<Pixel> out, Pixel val) const;
	};
	template<typename Pixel> struct MemSet2 {
		void operator()(std::span<Pixel> out, Pixel val0, Pixel val1) const;
	};

	[[nodiscard]] void* mallocAligned(size_t alignment, size_t size);
	void freeAligned(void* aligned);

} // namespace openmsx::MemoryOps

#endif
