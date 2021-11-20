#include "SuperImposedVideoFrame.hh"
#include "LineScalers.hh"
#include "MemoryOps.hh"
#include "vla.hh"
#include "build-info.hh"
#include <cstdint>

namespace openmsx {

template<typename Pixel>
SuperImposedVideoFrame<Pixel>::SuperImposedVideoFrame(
		const FrameSource& src_, const FrameSource& super_,
		const PixelOperations<Pixel>& pixelOps_)
	: FrameSource(pixelOps_.getPixelFormat())
	, src(src_), super(super_), pixelOps(pixelOps_)
{
	setHeight(src.getHeight());
}

template<typename Pixel>
unsigned SuperImposedVideoFrame<Pixel>::getLineWidth(unsigned line) const
{
	unsigned width = src.getLineWidth(line);
	return (width == 1) ? 320 : width;
}

template<typename Pixel>
const void* SuperImposedVideoFrame<Pixel>::getLineInfo(
	unsigned line, unsigned& width, void* buf1_, unsigned bufWidth) const
{
	auto* buf1 = static_cast<Pixel*>(buf1_);
	// Return minimum line width of 320.
	//  We could check whether both inputs have width=1 and in that case
	//  also return a line of width=1. But for now (laserdisc) this will
	//  never happen.
	auto* srcLine = static_cast<const Pixel*>(
		src.getLineInfo(line, width, buf1, bufWidth));
	if (width == 1) {
		width = 320;
		MemoryOps::MemSet<Pixel> memset;
		memset(buf1, 320, srcLine[0]);
		srcLine = buf1;
	}
	// (possibly) srcLine == buf1

	// Adjust the two inputs to the same height.
	VLA_SSE_ALIGNED(Pixel, buf2, width);
	assert(super.getHeight() == 480); // TODO possibly extend in the future
	const Pixel* supLine = [&]() -> const Pixel* {
		if (src.getHeight() == 240) {
			VLA_SSE_ALIGNED(Pixel, buf3, width);
			auto* sup0 = super.getLinePtr(2 * line + 0, width, buf2);
			auto* sup1 = super.getLinePtr(2 * line + 1, width, buf3);
			BlendLines<Pixel> blend(pixelOps);
			blend(sup0, sup1, buf2, width); // possibly sup0 == buf2
			return buf2;
		} else {
			assert(src.getHeight() == super.getHeight());
			return super.getLinePtr(line, width, buf2); // scale line
		}
	}();
	// (possibly) supLine == buf2

	// Actually blend the lines of both frames.
	AlphaBlendLines<Pixel> blend(pixelOps);
	blend(srcLine, supLine, buf1, width); // possibly srcLine == buf1
	return buf1;
}

// Force template instantiation.
#if HAVE_16BPP
template class SuperImposedVideoFrame<uint16_t>;
#endif
#if HAVE_32BPP
template class SuperImposedVideoFrame<uint32_t>;
#endif

} // namespace openmsx
