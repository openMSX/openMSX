// $Id$

#include "SuperImposedFrame.hh"
#include "LineScalers.hh"
#include "openmsx.hh"
#include "vla.hh"
#include "build-info.hh"

namespace openmsx {

template <typename Pixel>
SuperImposedFrame<Pixel>::SuperImposedFrame(
		const FrameSource& src_, const FrameSource& super_,
		const PixelOperations<Pixel>& pixelOps_)
	: FrameSource(pixelOps_.getSDLPixelFormat())
	, src(src_), super(super_), pixelOps(pixelOps_)
{
	setHeight(src.getHeight());
}

template <typename Pixel>
const void* SuperImposedFrame<Pixel>::getLineInfo(unsigned line, unsigned& width) const
{
	// Return minimum line width of 320.
	//  We could check whether both inputs have width=1 and in that case
	//  also return a line of width=1. But for now (laserdisc) this will
	//  never happen.
	width = src.getLineWidth(line);
	const Pixel* srcLine;
	if (width == 1) {
		width = 320;
		srcLine = src.getLinePtr<Pixel>(line, 320);
	} else {
		srcLine = src.getLinePtr<Pixel>(line);
	}

	// Adjust the two inputs to the same height.
	const Pixel* supLine;
	unsigned width2 = width; // workaround gcc-3.4 bug in VLA on next line
	VLA(Pixel, tmpBuf, width2);
	assert(super.getHeight() == 480); // TODO possibly extend in the future
	if (src.getHeight() == 240) {
		const Pixel* sup0 = super.getLinePtr<Pixel>(2 * line + 0, width);
		const Pixel* sup1 = super.getLinePtr<Pixel>(2 * line + 1, width);
		BlendLines<Pixel, 1, 1> blend(pixelOps);
		blend(sup0, sup1, tmpBuf, width);
		supLine = tmpBuf;
	} else {
		assert(src.getHeight() == super.getHeight());
		supLine = super.getLinePtr<Pixel>(line, width); // scale line
	}

	// Actually blend the lines of both frames.
	Pixel* resLine = static_cast<Pixel*>(src.getTempBuffer());
	AlphaBlendLines<Pixel> blend(pixelOps);
	blend(srcLine, supLine, resLine, width);
	return resLine;
}

// Force template instantiation.
#if HAVE_16BPP
template class SuperImposedFrame<word>;
#endif
#if HAVE_32BPP
template class SuperImposedFrame<unsigned>;
#endif

} // namespace openmsx
