#include "DirectScalerOutput.hh"
#include "OutputSurface.hh"
#include "MemoryOps.hh"
#include "build-info.hh"
#include <cstdint>

namespace openmsx {

template<typename Pixel>
DirectScalerOutput<Pixel>::DirectScalerOutput(OutputSurface& output_)
	: output(output_)
{
}

template<typename Pixel>
unsigned DirectScalerOutput<Pixel>::getWidth()  const
{
	return output.getWidth();
}

template<typename Pixel>
unsigned DirectScalerOutput<Pixel>::getHeight() const
{
	return output.getHeight();
}

template<typename Pixel>
Pixel* DirectScalerOutput<Pixel>::acquireLine(unsigned y)
{
	return output.getLinePtrDirect<Pixel>(y);
}

template<typename Pixel>
void DirectScalerOutput<Pixel>::releaseLine(unsigned /*y*/, Pixel* /*buf*/)
{
	// nothing
}

template<typename Pixel>
void DirectScalerOutput<Pixel>::fillLine(unsigned y, Pixel color)
{
	auto* dstLine = output.getLinePtrDirect<Pixel>(y);
	MemoryOps::MemSet<Pixel> memset;
	memset(dstLine, output.getWidth(), color);
}


// Force template instantiation.
#if HAVE_16BPP
template class DirectScalerOutput<uint16_t>;
#endif
#if HAVE_32BPP
template class DirectScalerOutput<uint32_t>;
#endif

} // namespace openmsx
