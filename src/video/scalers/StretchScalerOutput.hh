// $Id:$

#ifndef STRETCHSCALEROUTPUT_HH
#define STRETCHSCALEROUTPUT_HH

#include <memory>

namespace openmsx {

class OutputSurface;
template<typename Pixel> class PixelOperations;
template<typename Pixel> class ScalerOutput;

template<typename Pixel>
struct StretchScalerOutputFactory
{
	static std::auto_ptr<ScalerOutput<Pixel> > create(
		OutputSurface& output,
		const PixelOperations<Pixel>& pixelOps,
		unsigned inWidth);
};

} // namespace openmsx

#endif
