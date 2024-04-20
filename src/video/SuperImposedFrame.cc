#include "SuperImposedFrame.hh"
#include "LineScalers.hh"
#include "vla.hh"
#include <algorithm>
#include <cstdint>

namespace openmsx {

using Pixel = uint32_t;

void SuperImposedFrame::init(
	const FrameSource* top_, const FrameSource* bottom_)
{
	top = top_;
	bottom = bottom_;
	setHeight(std::max(top->getHeight(), bottom->getHeight()));
}

unsigned SuperImposedFrame::getLineWidth(unsigned line) const
{
	unsigned tNum = (getHeight() == top   ->getHeight()) ? line : line / 2;
	unsigned bNum = (getHeight() == bottom->getHeight()) ? line : line / 2;
	unsigned tWidth = top   ->getLineWidth(tNum);
	unsigned bWidth = bottom->getLineWidth(bNum);
	return std::max(tWidth, bWidth);
}

std::span<const FrameSource::Pixel> SuperImposedFrame::getUnscaledLine(
	unsigned line, void* buf, unsigned bufWidth) const
{
	unsigned tNum = (getHeight() == top   ->getHeight()) ? line : line / 2;
	unsigned bNum = (getHeight() == bottom->getHeight()) ? line : line / 2;
	unsigned tWidth = top   ->getLineWidth(tNum);
	unsigned bWidth = bottom->getLineWidth(bNum);
	unsigned width = std::min(std::max(tWidth, bWidth), // as wide as the widest source
	                          bufWidth); // but no wider than the output buffer

	auto tBuf = std::span{static_cast<Pixel*>(buf), width};
	VLA_SSE_ALIGNED(Pixel, bBuf, width);
	auto tLine = top   ->getLine(tNum, tBuf);
	auto bLine = bottom->getLine(bNum, bBuf);

	alphaBlendLines(tLine, bLine, tBuf); // possibly tLine == tBuf
	return tBuf;
}

} // namespace openmsx
