#include "SuperImposedFrame.hh"
#include "PixelOperations.hh"
#include "LineScalers.hh"
#include "memory.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include <algorithm>
#include <cstdint>

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

std::unique_ptr<SuperImposedFrame> SuperImposedFrame::create(
	const SDL_PixelFormat& format)
{
#if HAVE_16BPP
	if (format.BitsPerPixel == 15 || format.BitsPerPixel == 16) {
		return make_unique<SuperImposedFrameImpl<uint16_t>>(format);
	}
#endif
#if HAVE_32BPP
	if (format.BitsPerPixel == 32) {
		return make_unique<SuperImposedFrameImpl<uint32_t>>(format);
	}
#endif
	UNREACHABLE; return nullptr; // avoid warning
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
	unsigned tNum = (getHeight() == top   ->getHeight()) ? line : line / 2;
	unsigned bNum = (getHeight() == bottom->getHeight()) ? line : line / 2;
	unsigned tWidth = top   ->getLineWidth(tNum);
	unsigned bWidth = bottom->getLineWidth(bNum);
	return std::max(tWidth, bWidth);
}

template <typename Pixel>
const void* SuperImposedFrameImpl<Pixel>::getLineInfo(
	unsigned line, unsigned& width) const
{
	unsigned tNum = (getHeight() == top   ->getHeight()) ? line : line / 2;
	unsigned bNum = (getHeight() == bottom->getHeight()) ? line : line / 2;
	unsigned tWidth = top   ->getLineWidth(tNum);
	unsigned bWidth = bottom->getLineWidth(bNum);
	width = std::max(tWidth, bWidth);

	auto* tLine = top   ->getLinePtr<Pixel>(tNum, width);
	auto* bLine = bottom->getLinePtr<Pixel>(bNum, width);

	auto* output = static_cast<Pixel*>(getTempBuffer());
	AlphaBlendLines<Pixel> blend(pixelOps);
	blend(tLine, bLine, output, width);

	top   ->freeLineBuffers();
	bottom->freeLineBuffers();

	return output;
}

} // namespace openmsx
