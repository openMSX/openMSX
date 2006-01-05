// $Id$

#include "RawFrame.hh"
#include "MemoryOps.hh"
#include "openmsx.hh"
#include <SDL.h>

namespace openmsx {

RawFrame::RawFrame(
		const SDL_PixelFormat* format, unsigned maxWidth_, unsigned height)
	: FrameSource(format)
	, maxWidth(maxWidth_)
{
	setHeight(height);
	lineWidth = new unsigned[height];
	unsigned bytesPerPixel = format->BytesPerPixel;

	// Allocate memory, make sure each line begins at a 64 byte boundary
	//  SSE instruction need 16 byte aligned data
	//  cache line size on athlon and P4 CPUs is 64 bytes
	pitch = ((bytesPerPixel * maxWidth) + 63) & ~63;
	data = reinterpret_cast<char*>(
			MemoryOps::mallocAligned(64, pitch * height));

	// Start with a black frame.
	init(FIELD_NONINTERLACED);
	for (unsigned line = 0; line < height; line++) {
		if (bytesPerPixel == 2) {
			setBlank(line, static_cast<word>(0));
		} else {
			setBlank(line, static_cast<unsigned>(0));
		}
	}
}

RawFrame::~RawFrame()
{
	MemoryOps::freeAligned(data);
	delete[] lineWidth;
}

unsigned RawFrame::getLineBufferSize() const
{
	return pitch;
}

unsigned RawFrame::getLineWidth(unsigned line)
{
	assert(line < getHeight());
	return lineWidth[line];
}

void* RawFrame::getLinePtrImpl(unsigned line)
{
	return data + line * pitch;
}

} // namespace openmsx
