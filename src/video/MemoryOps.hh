// $Id$

#ifndef MEMORYOPS_HH
#define MEMORYOPS_HH

namespace openmsx {

namespace MemoryOps {

	static const bool NO_STREAMING = false;
	static const bool STREAMING    = true;

	template <typename Pixel, bool STREAMING> void memset(
		Pixel* out, unsigned num, Pixel val);
	template <typename Pixel, bool STREAMING> void memset_2(
		Pixel* out, unsigned num, Pixel val0, Pixel val1);

} // namespace MemoryOps

} // namespace openmsx

#endif
