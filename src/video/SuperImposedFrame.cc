#include "SuperImposedFrame.hh"
#include "PixelOperations.hh"
#include "LineScalers.hh"
#include "unreachable.hh"
#include "vla.hh"
#include <algorithm>
#include <concepts>
#include <cstdint>
#include <memory>

namespace openmsx {

template<std::unsigned_integral Pixel>
class SuperImposedFrameImpl final : public SuperImposedFrame
{
public:
	explicit SuperImposedFrameImpl(const PixelFormat& format);

private:
	[[nodiscard]] unsigned getLineWidth(unsigned line) const override;
	[[nodiscard]] const void* getLineInfo(
		unsigned line, unsigned& width,
		void* buf, unsigned bufWidth) const override;

	PixelOperations<Pixel> pixelOps;
};


// class SuperImposedFrame

std::unique_ptr<SuperImposedFrame> SuperImposedFrame::create(
	const PixelFormat& format)
{
	return std::make_unique<SuperImposedFrameImpl<uint32_t>>(format);
}

SuperImposedFrame::SuperImposedFrame(const PixelFormat& format)
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

template<std::unsigned_integral Pixel>
SuperImposedFrameImpl<Pixel>::SuperImposedFrameImpl(
		const PixelFormat& format)
	: SuperImposedFrame(format)
	, pixelOps(format)
{
}

template<std::unsigned_integral Pixel>
unsigned SuperImposedFrameImpl<Pixel>::getLineWidth(unsigned line) const
{
	unsigned tNum = (getHeight() == top   ->getHeight()) ? line : line / 2;
	unsigned bNum = (getHeight() == bottom->getHeight()) ? line : line / 2;
	unsigned tWidth = top   ->getLineWidth(tNum);
	unsigned bWidth = bottom->getLineWidth(bNum);
	return std::max(tWidth, bWidth);
}

template<std::unsigned_integral Pixel>
const void* SuperImposedFrameImpl<Pixel>::getLineInfo(
	unsigned line, unsigned& width, void* buf, unsigned bufWidth) const
{
	unsigned tNum = (getHeight() == top   ->getHeight()) ? line : line / 2;
	unsigned bNum = (getHeight() == bottom->getHeight()) ? line : line / 2;
	unsigned tWidth = top   ->getLineWidth(tNum);
	unsigned bWidth = bottom->getLineWidth(bNum);
	width = std::max(tWidth, bWidth);  // as wide as the widest source
	width = std::min(width, bufWidth); // but no wider than the output buffer

	auto tBuf = std::span{static_cast<Pixel*>(buf), width};
	VLA_SSE_ALIGNED(Pixel, bBuf, width);
	auto tLine = top   ->getLine(tNum, tBuf);
	auto bLine = bottom->getLine(bNum, bBuf);

	AlphaBlendLines<Pixel> blend(pixelOps);
	blend(tLine, bLine, tBuf); // possibly tLine == tBuf
	return tBuf.data();
}

} // namespace openmsx
