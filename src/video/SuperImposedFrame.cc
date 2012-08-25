// $Id$

#include "SuperImposedFrame.hh"
#include "PixelOperations.hh"
#include "LineScalers.hh"
#include "openmsx.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include <algorithm>

namespace openmsx {

template <typename Pixel>
class SuperImposedFrameImpl : public SuperImposedFrame
{
public:
	SuperImposedFrameImpl(const SDL_PixelFormat& format);

private:
	virtual unsigned getLineWidth(unsigned line) const;
	virtual const void* getLineInfo(unsigned line, unsigned& width) const;

	PixelOperations<Pixel> pixelOps;
};


// class SuperImposedFrame

std::auto_ptr<SuperImposedFrame> SuperImposedFrame::create(
	const SDL_PixelFormat& format)
{
#if HAVE_16BPP
	if (format.BitsPerPixel == 16) {
		return std::auto_ptr<SuperImposedFrame>(
			new SuperImposedFrameImpl<word>(format));
	}
#endif
#if HAVE_32BPP
	if (format.BitsPerPixel == 32) {
		return std::auto_ptr<SuperImposedFrame>(
			new SuperImposedFrameImpl<unsigned>(format));
	}
#endif
	UNREACHABLE;
	return std::auto_ptr<SuperImposedFrame>(); // avoid warning
}

SuperImposedFrame::SuperImposedFrame(const SDL_PixelFormat& format)
	: FrameSource(format)
{
}

void SuperImposedFrame::init(
	const FrameSource* top_, const FrameSource* bottom_)
{
	top = top_;
	bottom = bottom_;
	setHeight(std::max(top->getHeight(), bottom->getHeight()));
}


// class SuperImposedFrameImpl

template <typename Pixel>
SuperImposedFrameImpl<Pixel>::SuperImposedFrameImpl(
		const SDL_PixelFormat& format)
	: SuperImposedFrame(format)
	, pixelOps(format)
{
}

template <typename Pixel>
unsigned SuperImposedFrameImpl<Pixel>::getLineWidth(unsigned line) const
{
	unsigned tShift = (getHeight() == top   ->getHeight()) ? 0 : 1;
	unsigned bShift = (getHeight() == bottom->getHeight()) ? 0 : 1;
	unsigned tWidth = top   ->getLineWidth(line >> tShift);
	unsigned bWidth = bottom->getLineWidth(line >> bShift);
	return std::max(tWidth, bWidth);
}

template <typename Pixel>
const void* SuperImposedFrameImpl<Pixel>::getLineInfo(
	unsigned line, unsigned& width) const
{
	unsigned tShift = (getHeight() == top   ->getHeight()) ? 0 : 1;
	unsigned bShift = (getHeight() == bottom->getHeight()) ? 0 : 1;

	unsigned tWidth = top   ->getLineWidth(line >> tShift);
	unsigned bWidth = bottom->getLineWidth(line >> bShift);
	width = std::max(tWidth, bWidth);

	const Pixel* tLine = top   ->getLinePtr<Pixel>(line >> tShift, width);
	const Pixel* bLine = bottom->getLinePtr<Pixel>(line >> bShift, width);

	Pixel* output = static_cast<Pixel*>(getTempBuffer());
	AlphaBlendLines<Pixel> blend(pixelOps);
	blend(tLine, bLine, output, width);

	top   ->freeLineBuffers();
	bottom->freeLineBuffers();

	return output;
}

} // namespace openmsx
