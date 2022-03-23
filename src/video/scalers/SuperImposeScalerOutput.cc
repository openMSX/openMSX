#include "SuperImposeScalerOutput.hh"
#include "RawFrame.hh"
#include "LineScalers.hh"
#include "MemoryOps.hh"
#include "unreachable.hh"
#include "vla.hh"
#include "build-info.hh"
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel>
SuperImposeScalerOutput<Pixel>::SuperImposeScalerOutput(
		ScalerOutput<Pixel>& output_,
		const RawFrame& superImpose_,
		const PixelOperations<Pixel>& pixelOps_)
	: output(output_)
	, superImpose(superImpose_)
	, pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
unsigned SuperImposeScalerOutput<Pixel>::getWidth() const
{
	return output.getWidth();
}

template<std::unsigned_integral Pixel>
unsigned SuperImposeScalerOutput<Pixel>::getHeight() const
{
	return output.getHeight();
}

template<std::unsigned_integral Pixel>
Pixel* SuperImposeScalerOutput<Pixel>::acquireLine(unsigned y)
{
	return output.acquireLine(y);
}

template<std::unsigned_integral Pixel>
void SuperImposeScalerOutput<Pixel>::releaseLine(unsigned y, Pixel* buf)
{
	unsigned width = output.getWidth();
	VLA_SSE_ALIGNED(Pixel, buf2, width);
	auto* srcLine = getSrcLine(y, buf2);
	AlphaBlendLines<Pixel> alphaBlend(pixelOps);
	alphaBlend(buf, srcLine, buf, width);
	output.releaseLine(y, buf);
}

template<std::unsigned_integral Pixel>
void SuperImposeScalerOutput<Pixel>::fillLine(unsigned y, Pixel color)
{
	auto* dstLine = output.acquireLine(y);
	unsigned width = output.getWidth();
	if (pixelOps.isFullyOpaque(color)) {
		MemoryOps::MemSet<Pixel> memset;
		memset(dstLine, width, color);
	} else {
		auto* srcLine = getSrcLine(y, dstLine);
		if (pixelOps.isFullyTransparent(color)) {
			// optimization: use destination as work buffer, in case
			// that buffer got used, we don't need to make a copy
			// anymore
			if (srcLine != dstLine) {
				Scale_1on1<Pixel> copy;
				copy(srcLine, dstLine, width);
			}
		} else {
			AlphaBlendLines<Pixel> alphaBlend(pixelOps);
			alphaBlend(color, srcLine, dstLine, width); // possibly srcLine == dstLine
		}
	}
	output.releaseLine(y, dstLine);
}

template<std::unsigned_integral Pixel>
const Pixel* SuperImposeScalerOutput<Pixel>::getSrcLine(unsigned y, Pixel* buf)
{
	unsigned width = output.getWidth();
	if (width == 320) {
		return superImpose.getLinePtr320_240(y, buf);
	} else if (width == 640) {
		return superImpose.getLinePtr640_480(y, buf);
	} else if (width == 960) {
		return superImpose.getLinePtr960_720(y, buf);
	} else {
		UNREACHABLE; return nullptr;
	}
}


// Force template instantiation.
#if HAVE_16BPP
template class SuperImposeScalerOutput<uint16_t>;
#endif
#if HAVE_32BPP
template class SuperImposeScalerOutput<uint32_t>;
#endif

} // namespace openmsx
