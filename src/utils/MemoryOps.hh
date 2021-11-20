#ifndef MEMORYOPS_HH
#define MEMORYOPS_HH

#include <cstddef>

namespace openmsx::MemoryOps {

	template<typename Pixel> struct MemSet {
		void operator()(Pixel* out, size_t num,
		                Pixel val) const;
	};
	template<typename Pixel> struct MemSet2 {
		void operator()(Pixel* out, size_t num,
		                Pixel val0, Pixel val1) const;
	};

	[[nodiscard]] void* mallocAligned(size_t alignment, size_t size);
	void freeAligned(void* aligned);

} // namespace openmsx::MemoryOps

#endif
