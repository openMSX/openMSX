// $Id$

#ifndef MEMORYOPS_HH
#define MEMORYOPS_HH

#include "openmsx.hh"

namespace openmsx {

namespace MemoryOps {

	static const bool NO_STREAMING = false;
	static const bool STREAMING    = true;

	template <typename Pixel, bool STREAMING> void memset(
		Pixel* out, unsigned num, Pixel val);
	template <typename Pixel, bool STREAMING> void memset_2(
		Pixel* out, unsigned num, Pixel val0, Pixel val1);

	/** Perform memcpy with streaming stores.
	  * @param dst Destination address, must be aligned on unsigned/word
	  *            boundary.
	  * @param src Source address, must be aligned on unsigned/word
	  *            boundary. 
	  * @param num Number of elements (unsigned/word). Notice this is
	  *            different from the normal memcpy function, there this
	  *            parameter indicates the number of bytes.
	  */
	void stream_memcpy(unsigned* dst, const unsigned* src, unsigned num);
	void stream_memcpy(word* dst, const word* src, unsigned num);

	void* mallocAligned(unsigned alignment, unsigned size);
	void freeAligned(void* ptr);

} // namespace MemoryOps

} // namespace openmsx

#endif
