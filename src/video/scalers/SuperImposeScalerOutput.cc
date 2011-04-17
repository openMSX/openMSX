// $Id:$

#include "SuperImposeScalerOutput.hh"
#include "RawFrame.hh"
#include "LineScalers.hh"
#include "MemoryOps.hh"
#include "unreachable.hh"
#include "build-info.hh"

namespace openmsx {

template<typename Pixel>
SuperImposeScalerOutput<Pixel>::SuperImposeScalerOutput(
		ScalerOutput<Pixel>& output_,
		const RawFrame& superImpose_,
		const PixelOperations<Pixel>& pixelOps_)
	: output(output_)
	, superImpose(superImpose_)
	, pixelOps(pixelOps_)
{
}

template<typename Pixel>
unsigned SuperImposeScalerOutput<Pixel>::getWidth() const
{
	return output.getWidth();
}

template<typename Pixel>
unsigned SuperImposeScalerOutput<Pixel>::getHeight() const
{
	return output.getHeight();
}

template<typename Pixel>
Pixel* SuperImposeScalerOutput<Pixel>::acquireLine(unsigned y)
{
	return output.acquireLine(y);
}

template<typename Pixel>
void SuperImposeScalerOutput<Pixel>::releaseLine(unsigned y, Pixel* buf)
{
	const Pixel* srcLine = getSrcLine(y);
	AlphaBlendLines<Pixel> alphaBlend(pixelOps);
	alphaBlend(buf, srcLine, buf, output.getWidth());
	superImpose.freeLineBuffers();
	output.releaseLine(y, buf);
}

template<typename Pixel>
void SuperImposeScalerOutput<Pixel>::fillLine(unsigned y, Pixel color)
{
	Pixel* dstLine = output.acquireLine(y);
	unsigned width = this->getWidth();
	if (pixelOps.isFullyOpaque(color)) {
		MemoryOps::MemSet<Pixel, MemoryOps::STREAMING> memset;
		memset(dstLine, width, color);
	} else {
		const Pixel* srcLine = getSrcLine(y);
		if (pixelOps.isFullyTransparent(color)) {
			Scale_1on1<Pixel> copy;
			copy(srcLine, dstLine, width);
		} else {
			AlphaBlendLines<Pixel> alphaBlend(pixelOps);
			alphaBlend(color, srcLine, dstLine, width);
		}
		superImpose.freeLineBuffers();
	}
	output.releaseLine(y, dstLine);
}

template<typename Pixel>
const Pixel* SuperImposeScalerOutput<Pixel>::getSrcLine(unsigned y)
{
	unsigned width = output.getWidth();
	if (width == 320) {
		return superImpose.getLinePtr320_240<Pixel>(y);
	} else if (width == 640) {
		return superImpose.getLinePtr640_480<Pixel>(y);
	} else if (width == 960) {
		return superImpose.getLinePtr960_720<Pixel>(y);
	} else {
		UNREACHABLE; return 0;
	}
}


// Force template instantiation.
#if HAVE_16BPP
template class SuperImposeScalerOutput<word>;
#endif
#if HAVE_32BPP
template class SuperImposeScalerOutput<unsigned>;
#endif

} // namespace openmsx
