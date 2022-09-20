#include "SDLOffScreenSurface.hh"
#include "SDLVisibleSurface.hh"
#include <cstring>

namespace openmsx {

SDLOffScreenSurface::SDLOffScreenSurface(const SDL_Surface& proto)
{
	gl::ivec2 size(proto.w, proto.h);
	calculateViewPort(size, size);
	setSDLPixelFormat(*proto.format);

	// SDL_CreateRGBSurface() allocates an internal buffer, on 32-bit
	// systems this buffer is only 8-bytes aligned. For some scalers (with
	// SSE(2) optimizations) we need a 16-byte aligned buffer. So now we
	// allocate the buffer ourselves and create the SDL_Surface with
	// SDL_CreateRGBSurfaceFrom().
	// Of course it would be better to get rid of SDL_Surface in the
	// OutputSurface interface.

	const PixelFormat& frmt = getPixelFormat();
	unsigned pitch2 = proto.w * frmt.getBytesPerPixel();
	assert((pitch2 % 16) == 0);
	unsigned bufSize = pitch2 * proto.h;
	buffer.resize(bufSize);
	ranges::fill(std::span{buffer.data(), bufSize}, 0);
	surface.reset(SDL_CreateRGBSurfaceFrom(
		buffer.data(), proto.w, proto.h, frmt.getBpp(), pitch2,
		frmt.getRmask(), frmt.getGmask(), frmt.getBmask(), frmt.getAmask()));

	setSDLSurface(surface.get());

	// Used (only?) by 'screenshot -with-osd'.
	renderer.reset(SDL_CreateSoftwareRenderer(surface.get()));
	setSDLRenderer(renderer.get());
}

void SDLOffScreenSurface::saveScreenshot(const std::string& filename)
{
	SDLVisibleSurface::saveScreenshotSDL(*this, filename);
}

void SDLOffScreenSurface::clearScreen()
{
	memset(surface->pixels, 0, uint32_t(surface->pitch) * surface->h);
}

} // namespace openmsx
