#include "RawFrame.hh"
#include "MemoryOps.hh"
#include "MSXException.hh"
#include <cstdint>
#include <SDL.h>

#if PLATFORM_GP2X
#include "GP2XMMUHack.hh"
#endif

namespace openmsx {

RawFrame::RawFrame(
		const SDL_PixelFormat& format, unsigned maxWidth_, unsigned height)
	: FrameSource(format)
	, lineWidths(height)
	, maxWidth(maxWidth_)
{
	setHeight(height);
	unsigned bytesPerPixel = format.BytesPerPixel;

	if (PLATFORM_GP2X) {
		surface.reset(SDL_CreateRGBSurface(
			SDL_HWSURFACE, maxWidth, height, format.BitsPerPixel,
			format.Rmask, format.Gmask, format.Bmask, format.Amask));
		if (!surface.get()) {
			throw FatalError("Couldn't allocate surface.");
		}
		// To keep code more uniform with non-GP2X case, we copy these
		// two variables. This is OK if the variables inside surface
		// don't change. AFAIK SDL doesn't explictly document this, but
		// I don't see why they would change.
		// Note: for a double buffered display surface, the pixels
		// variable DOES change.
		data = static_cast<char*>(surface->pixels);
		pitch = surface->pitch;
	} else {
		// Allocate memory, make sure each line begins at a 64 byte
		// boundary:
		//  - SSE instruction need 16 byte aligned data
		//  - cache line size on athlon and P4 CPUs is 64 bytes
		pitch = ((bytesPerPixel * maxWidth) + 63) & ~63;
		data = reinterpret_cast<char*>(
		           MemoryOps::mallocAligned(64, pitch * height));
	}
	maxWidth = pitch / bytesPerPixel; // adjust maxWidth
	locked = false;

	// Start with a black frame.
	init(FIELD_NONINTERLACED);
	for (unsigned line = 0; line < height; line++) {
		if (bytesPerPixel == 2) {
			setBlank(line, static_cast<uint16_t>(0));
		} else {
			setBlank(line, static_cast<uint32_t>(0));
		}
	}

#if PLATFORM_GP2X
	GP2XMMUHack::instance().patchPageTables();
#endif
}

RawFrame::~RawFrame()
{
	if (PLATFORM_GP2X) {
		// surface is freed automatically
	} else {
		MemoryOps::freeAligned(data);
	}
}

unsigned RawFrame::getLineWidth(unsigned line) const
{
	assert(line < getHeight());
	return lineWidths[line];
}

const void* RawFrame::getLineInfo(unsigned line, unsigned& width) const
{
	if (PLATFORM_GP2X) {
		if (!isLocked()) const_cast<RawFrame*>(this)->lock();
	}
	assert(line < getHeight());
	width = lineWidths[line];
	return data + line * pitch;
}

unsigned RawFrame::getRowLength() const
{
	return maxWidth; // in pixels (not in bytes)
}

bool RawFrame::hasContiguousStorage() const
{
	return true;
}

void RawFrame::lock()
{
	if (isLocked()) return;
	locked = true;
	if (SDL_MUSTLOCK(surface.get())) SDL_LockSurface(surface.get());
	// Note: we ignore the return value from SDL_LockSurface()
}

void RawFrame::unlock()
{
	if (!isLocked()) return;
	locked = false;
	if (SDL_MUSTLOCK(surface.get())) SDL_UnlockSurface(surface.get());
}

} // namespace openmsx
