#include "RawFrame.hh"
#include <cstdint>
#include <SDL.h>

namespace openmsx {

RawFrame::RawFrame(
		const SDL_PixelFormat& format, unsigned maxWidth_, unsigned height_)
	: FrameSource(format)
	, lineWidths(height_)
	, maxWidth(maxWidth_)
{
	setHeight(height_);
	unsigned bytesPerPixel = format.BytesPerPixel;

	// Allocate memory, make sure each line starts at a 64 byte boundary:
	// - SSE instructions need 16 byte aligned data
	// - cache line size on many CPUs is 64 bytes
	pitch = ((bytesPerPixel * maxWidth) + 63) & ~63;
	data.resize(pitch * height_);

	maxWidth = pitch / bytesPerPixel; // adjust maxWidth

	// Start with a black frame.
	init(FIELD_NONINTERLACED);
	for (unsigned line = 0; line < height_; line++) {
		if (bytesPerPixel == 2) {
			setBlank(line, static_cast<uint16_t>(0));
		} else {
			setBlank(line, static_cast<uint32_t>(0));
		}
	}
}

unsigned RawFrame::getLineWidth(unsigned line) const
{
	assert(line < getHeight());
	return lineWidths[line];
}

const void* RawFrame::getLineInfo(
	unsigned line, unsigned& width,
	void* /*buf*/, unsigned /*bufWidth*/) const
{
	assert(line < getHeight());
	width = lineWidths[line];
	return data.data() + line * pitch;
}

unsigned RawFrame::getRowLength() const
{
	return maxWidth; // in pixels (not in bytes)
}

bool RawFrame::hasContiguousStorage() const
{
	return true;
}

} // namespace openmsx
