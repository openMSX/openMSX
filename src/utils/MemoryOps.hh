#ifndef MEMORYOPS_HH
#define MEMORYOPS_HH

#include "openmsx.hh"
#include <cstring> // for size_t
#include <cstdint>

namespace openmsx {
namespace MemoryOps {

	template <typename Pixel> struct MemSet {
		void operator()(Pixel* out, size_t num,
		                Pixel val) const;
	};
	template <typename Pixel> struct MemSet2 {
		void operator()(Pixel* out, size_t num,
		                Pixel val0, Pixel val1) const;
	};

	/** Perform memcpy with streaming stores.
	  * @param dst Destination address, must be aligned on uint32_t/uint16_t
	  *            boundary.
	  * @param src Source address, must be aligned on uint32_t/uint16_t
	  *            boundary.
	  * @param num Number of elements (uint32_t/uint16_t). Notice this is
	  *            different from the normal memcpy function, there this
	  *            parameter indicates the number of bytes.
	  */
	void stream_memcpy(uint32_t* dst, const uint32_t* src, size_t num);
	void stream_memcpy(uint16_t* dst, const uint16_t* src, size_t num);

	void* mallocAligned(size_t alignment, size_t size);
	void freeAligned(void* ptr);

} // namespace MemoryOps
} // namespace openmsx

#endif
