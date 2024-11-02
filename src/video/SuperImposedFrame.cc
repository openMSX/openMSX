#include "SuperImposedFrame.hh"

#include "LineScalers.hh"

#include "inplace_buffer.hh"

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
	unsigned line, std::span<Pixel> helpBuf) const
{
	unsigned tNum = (getHeight() == top   ->getHeight()) ? line : line / 2;
	unsigned bNum = (getHeight() == bottom->getHeight()) ? line : line / 2;
	unsigned tWidth = top   ->getLineWidth(tNum);
	unsigned bWidth = bottom->getLineWidth(bNum);
	auto width = std::min(std::max<size_t>(tWidth, bWidth), // as wide as the widest source
	                      helpBuf.size()); // but no wider than the output buffer

	auto tBuf = helpBuf.subspan(0, width);
	inplace_buffer<Pixel, 1280> bBuf(uninitialized_tag{}, width);
	auto tLine = top   ->getLine(narrow<int>(tNum), tBuf);
	auto bLine = bottom->getLine(narrow<int>(bNum), bBuf);

	alphaBlendLines(tLine, bLine, tBuf); // possibly tLine == tBuf
	return tBuf;
}

} // namespace openmsx
