#include "SDLOutputSurface.hh"
#include "unreachable.hh"
#include "build-info.hh"

namespace openmsx {

void SDLOutputSurface::lock()
{
	if (isLocked()) return;
	locked = true;
	if (surface && SDL_MUSTLOCK(surface)) {
		// Note: we ignore the return value from SDL_LockSurface()
		SDL_LockSurface(surface);
	}
}

void SDLOutputSurface::unlock()
{
	if (!isLocked()) return;
	locked = false;
	if (surface && SDL_MUSTLOCK(surface)) {
		SDL_UnlockSurface(surface);
	}
}

void SDLOutputSurface::setSDLPixelFormat(const SDL_PixelFormat& format)
{
	auto bpp = format.BitsPerPixel;

	auto Rmask = format.Rmask;
	auto Rshift = format.Rshift;
	auto Rloss = format.Rloss;

	auto Gmask = format.Gmask;
	auto Gshift = format.Gshift;
	auto Gloss = format.Gloss;

	auto Bmask = format.Bmask;
	auto Bshift = format.Bshift;
	auto Bloss = format.Bloss;

	auto Amask = format.Amask;
	auto Ashift = format.Ashift;
	auto Aloss = format.Aloss;

	// TODO is this still needed with SDL2?
	// SDL sets an alpha channel only for GL modes. We want an alpha channel
	// for all 32bpp output surfaces, so we add one ourselves if necessary.
	if (bpp == 32 && Amask == 0) {
		unsigned rgbMask = Rmask | Gmask | Bmask;
		if ((rgbMask & 0x000000FF) == 0) {
			Amask  = 0x000000FF;
			Ashift = 0;
			Aloss  = 0;
		} else if ((rgbMask & 0xFF000000) == 0) {
			Amask  = 0xFF000000;
			Ashift = 24;
			Aloss  = 0;
		} else {
			UNREACHABLE;
		}
	}

	setPixelFormat(PixelFormat(bpp,
	                           Rmask, Rshift, Rloss,
	                           Gmask, Gshift, Gloss,
	                           Bmask, Bshift, Bloss,
	                           Amask, Ashift, Aloss));
}

void SDLOutputSurface::setOpenGlPixelFormat()
{
	setPixelFormat(PixelFormat(
		32,
		OPENMSX_BIGENDIAN ? 0xFF000000 : 0x00FF0000, OPENMSX_BIGENDIAN ? 24 : 16, 0,
		OPENMSX_BIGENDIAN ? 0x00FF0000 : 0x0000FF00, OPENMSX_BIGENDIAN ? 16 :  8, 0,
		OPENMSX_BIGENDIAN ? 0x0000FF00 : 0x000000FF, OPENMSX_BIGENDIAN ?  8 :  0, 0,
		OPENMSX_BIGENDIAN ? 0x000000FF : 0xFF000000, OPENMSX_BIGENDIAN ?  0 : 24, 0));
}

void SDLOutputSurface::setBufferPtr(char* data_, unsigned pitch_)
{
	data = data_;
	pitch = pitch_;
}

} // namespace openmsx
