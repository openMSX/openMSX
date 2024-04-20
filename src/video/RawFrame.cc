#include "RawFrame.hh"
#include "narrow.hh"
#include <cstdint>

namespace openmsx {

RawFrame::RawFrame(unsigned maxWidth_, unsigned height_)
	: lineWidths(height_)
	, maxWidth(maxWidth_)
	, pitch(narrow<unsigned>(((sizeof(Pixel) * maxWidth) + 63) & ~63))
{
	setHeight(height_);

	// Allocate memory, make sure each line starts at a 64 byte boundary:
	// - SSE instructions need 16 byte aligned data
	// - cache line size on many CPUs is 64 bytes
	data.resize(size_t(pitch) * height_);

	maxWidth = pitch / sizeof(Pixel); // adjust maxWidth

	// Start with a black frame.
	init(FIELD_NONINTERLACED);
	for (auto line : xrange(height_)) {
		setBlank(line, static_cast<uint32_t>(0));
	}
}

unsigned RawFrame::getLineWidth(unsigned line) const
{
	assert(line < getHeight());
	return lineWidths[line];
}

std::span<const RawFrame::Pixel> RawFrame::getUnscaledLine(
	unsigned line, std::span<Pixel> /*helpBuf*/) const
{
	assert(line < getHeight());
	return std::span{std::bit_cast<const Pixel*>(data.data() + line * size_t(pitch)),
		         lineWidths[line]};
}

bool RawFrame::hasContiguousStorage() const
{
	return true;
}

} // namespace openmsx
