#include "DirectScalerOutput.hh"
#include "SDLOutputSurface.hh"
#include "MemoryOps.hh"
#include "build-info.hh"
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel>
DirectScalerOutput<Pixel>::DirectScalerOutput(SDLOutputSurface& output_)
	: output(output_), pixelAccess(output.getDirectPixelAccess())
{
}

template<std::unsigned_integral Pixel>
unsigned DirectScalerOutput<Pixel>::getWidth()  const
{
	return output.getLogicalWidth();
}

template<std::unsigned_integral Pixel>
unsigned DirectScalerOutput<Pixel>::getHeight() const
{
	return output.getLogicalHeight();
}

template<std::unsigned_integral Pixel>
Pixel* DirectScalerOutput<Pixel>::acquireLine(unsigned y)
{
	return pixelAccess.getLinePtr<Pixel>(y);
}

template<std::unsigned_integral Pixel>
void DirectScalerOutput<Pixel>::releaseLine(unsigned /*y*/, Pixel* /*buf*/)
{
	// nothing
}

template<std::unsigned_integral Pixel>
void DirectScalerOutput<Pixel>::fillLine(unsigned y, Pixel color)
{
	auto* dstLine = pixelAccess.getLinePtr<Pixel>(y);
	MemoryOps::MemSet<Pixel> memset;
	memset(dstLine, output.getLogicalWidth(), color);
}


// Force template instantiation.
#if HAVE_16BPP
template class DirectScalerOutput<uint16_t>;
#endif
#if HAVE_32BPP
template class DirectScalerOutput<uint32_t>;
#endif

} // namespace openmsx
