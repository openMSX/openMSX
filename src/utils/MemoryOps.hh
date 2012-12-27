// $Id$

#ifndef MEMORYOPS_HH
#define MEMORYOPS_HH

#include "openmsx.hh"
#include <cstring> // for size_t

namespace openmsx {
namespace MemoryOps {

	static const bool NO_STREAMING = false;
	static const bool STREAMING    = true;

	template <typename Pixel, bool STREAMING> struct MemSet {
		void operator()(Pixel* out, size_t num, Pixel val) const;
	};
	template <typename Pixel, bool STREAMING> struct MemSet2 {
		void operator()(Pixel* out, size_t num,
		                Pixel val0, Pixel val1) const;
	};

	/** Perform memcpy with streaming stores.
	  * @param dst Destination address, must be aligned on unsigned/word
	  *            boundary.
	  * @param src Source address, must be aligned on unsigned/word
	  *            boundary.
	  * @param num Number of elements (unsigned/word). Notice this is
	  *            different from the normal memcpy function, there this
	  *            parameter indicates the number of bytes.
	  */
	void stream_memcpy(unsigned* dst, const unsigned* src, size_t num);
	void stream_memcpy(word* dst, const word* src, size_t num);

	void* mallocAligned(size_t alignment, size_t size);
	void freeAligned(void* ptr);

} // namespace MemoryOps
} // namespace openmsx

#endif
