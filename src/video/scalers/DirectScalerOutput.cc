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
std::span<Pixel> DirectScalerOutput<Pixel>::acquireLine(unsigned y)
{
	return pixelAccess.getLine<Pixel>(y).subspan(0, getWidth());
}

template<std::unsigned_integral Pixel>
void DirectScalerOutput<Pixel>::releaseLine(unsigned y, std::span<Pixel> buf)
{
	assert(buf.data() == pixelAccess.getLine<Pixel>(y).data());
	assert(buf.size() == getWidth());
	(void)y;
	(void)buf;
}

template<std::unsigned_integral Pixel>
void DirectScalerOutput<Pixel>::fillLine(unsigned y, Pixel color)
{
	MemoryOps::MemSet<Pixel> memset;
	auto dstLine = pixelAccess.getLine<Pixel>(y).subspan(0, getWidth());
	memset(dstLine, color);
}


// Force template instantiation.
#if HAVE_16BPP
template class DirectScalerOutput<uint16_t>;
#endif
#if HAVE_32BPP
template class DirectScalerOutput<uint32_t>;
#endif

} // namespace openmsx
