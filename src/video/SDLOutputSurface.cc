#include "SDLOutputSurface.hh"
#include "unreachable.hh"

namespace openmsx {

SDLDirectPixelAccess::SDLDirectPixelAccess(SDL_Surface* surface_)
	: surface(surface_)
{
	assert(surface);
	if (SDL_MUSTLOCK(surface)) {
		// Note: we ignore the return value from SDL_LockSurface()
		SDL_LockSurface(surface);
	}
}

SDLDirectPixelAccess::~SDLDirectPixelAccess()
{
	if (SDL_MUSTLOCK(surface)) {
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

} // namespace openmsx
