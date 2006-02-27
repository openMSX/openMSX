// $Id$

#include "SDLVisibleSurface.hh"
#include "ScreenShotSaver.hh"
#include "CommandException.hh"
#include "SDLSnow.hh"
#include "SDLConsole.hh"
#include "IconLayer.hh"
#include <cstring>

namespace openmsx {

SDLVisibleSurface::SDLVisibleSurface(
		unsigned width, unsigned height, bool fullscreen)
{
	int flags = SDL_SWSURFACE | (fullscreen ? SDL_FULLSCREEN : 0);
	createSurface(width, height, flags);
	memcpy(&format, surface->format, sizeof(SDL_PixelFormat));

	data = (char*)surface->pixels;
	pitch = surface->pitch;
}

bool SDLVisibleSurface::init()
{
	if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) < 0) {
		return false;
	}
	return true;
}

void SDLVisibleSurface::drawFrameBuffer()
{
	// nothing
}

void SDLVisibleSurface::finish()
{
	if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
	SDL_Flip(surface);
}

void SDLVisibleSurface::takeScreenShot(const std::string& filename)
{
	if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) < 0) {
		throw CommandException("Failed to lock surface.");
	}
	try {
		ScreenShotSaver::save(surface, filename);
		if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
	} catch (CommandException& e) {
		if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
		throw;
	}
}

std::auto_ptr<Layer> SDLVisibleSurface::createSnowLayer()
{
	switch (getFormat()->BytesPerPixel) {
	case 2:
		return std::auto_ptr<Layer>(new SDLSnow<Uint16>(surface));
	case 4:
		return std::auto_ptr<Layer>(new SDLSnow<Uint32>(surface));
	default:
		assert(false);
		return std::auto_ptr<Layer>(); // avoid warning
	}
}

std::auto_ptr<Layer> SDLVisibleSurface::createConsoleLayer(
		Reactor& reactor)
{
	return std::auto_ptr<Layer>(new SDLConsole(reactor, surface));
}

std::auto_ptr<Layer> SDLVisibleSurface::createIconLayer(
		CommandController& commandController,
		Display& display, IconStatus& iconStatus)
{
	return std::auto_ptr<Layer>(new SDLIconLayer(
			commandController, display, iconStatus, surface));
}

} // namespace openmsx
