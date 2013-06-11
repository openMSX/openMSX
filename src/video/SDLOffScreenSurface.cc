#include "SDLOffScreenSurface.hh"
#include "PNG.hh"
#include "MemoryOps.hh"
#include <cstring>

namespace openmsx {

SDLOffScreenSurface::SDLOffScreenSurface(const SDL_Surface& proto)
{
	// SDL_CreateRGBSurface() allocates an internal buffer, on 32-bit
	// systems this buffer is only 8-bytes aligned. For some scalers (with
	// SSE(2) optimizations) we need a 16-byte aligned buffer. So now we
	// allocate the buffer ourselves and create the SDL_Surface with
	// SDL_CreateRGBSurfaceFrom().
	// Of course it would be better to get rid of SDL_Surface in the
	// OutputSurface interface.

	setSDLFormat(*proto.format);
	const SDL_PixelFormat& format = getSDLFormat();

	unsigned pitch = proto.w * format.BitsPerPixel / 8;
	assert((pitch % 16) == 0);
	unsigned size = pitch * proto.h;
	buffer = MemoryOps::mallocAligned(16, size);
	memset(buffer, 0, size);
	surface.reset(SDL_CreateRGBSurfaceFrom(
		buffer, proto.w, proto.h, format.BitsPerPixel, pitch,
		format.Rmask, format.Gmask, format.Bmask, format.Amask));

	setSDLSurface(surface.get());
	setBufferPtr(static_cast<char*>(surface->pixels), surface->pitch);
}

SDLOffScreenSurface::~SDLOffScreenSurface()
{
	surface.reset();
	MemoryOps::freeAligned(buffer);
}

void SDLOffScreenSurface::saveScreenshot(const std::string& filename)
{
	lock();
	PNG::save(getSDLSurface(), filename);
}

} // namespace openmsx
