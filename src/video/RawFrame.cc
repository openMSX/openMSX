#include "RawFrame.hh"

namespace openmsx {

[[nodiscard]] static unsigned calcMaxWidth(unsigned maxWidth)
{
	unsigned bytes = maxWidth * sizeof(RawFrame::Pixel);
	bytes = (bytes + 63) & ~63; // round up to cache-line size
	return bytes / sizeof(RawFrame::Pixel);
}

RawFrame::RawFrame(unsigned maxWidth_, unsigned height_)
	: lineWidths(height_)
	, maxWidth(calcMaxWidth(maxWidth_))
{
	setHeight(height_);

	// Allocate memory, make sure each line starts at a 64 byte boundary:
	// - SSE instructions need 16 byte aligned data
	// - cache line size on many CPUs is 64 bytes
	data.resize(size_t(maxWidth) * height_);

	// Start with a black frame.
	init(FieldType::NONINTERLACED);
	for (auto line : xrange(height_)) {
		setBlank(line, Pixel(0));
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
	return getLineDirect(line).first(lineWidths[line]);
}

bool RawFrame::hasContiguousStorage() const
{
	return true;
}

} // namespace openmsx
