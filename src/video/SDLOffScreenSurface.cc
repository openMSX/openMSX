#include "SDLOffScreenSurface.hh"
#include "PNG.hh"
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
	const SDL_PixelFormat& frmt = getSDLFormat();

	unsigned pitch2 = proto.w * frmt.BitsPerPixel / 8;
	assert((pitch2 % 16) == 0);
	unsigned size = pitch2 * proto.h;
	buffer.resize(size);
	memset(buffer.data(), 0, size);
	surface.reset(SDL_CreateRGBSurfaceFrom(
		buffer.data(), proto.w, proto.h, frmt.BitsPerPixel, pitch2,
		frmt.Rmask, frmt.Gmask, frmt.Bmask, frmt.Amask));

	setSDLSurface(surface.get());
	setBufferPtr(static_cast<char*>(surface->pixels), surface->pitch);
}

void SDLOffScreenSurface::saveScreenshot(const std::string& filename)
{
	lock();
	PNG::save(getSDLSurface(), filename);
}

void SDLOffScreenSurface::clearScreen()
{
	memset(surface->pixels, 0, uint32_t(surface->pitch) * surface->h);
}

} // namespace openmsx
