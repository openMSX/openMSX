// $Id$

#include "SDLOutputSurface.hh"
#include "ScreenShotSaver.hh"
#include "CommandException.hh"
#include "SDLSnow.hh"
#include "SDLConsole.hh"
#include "IconLayer.hh"
#include <cstring>

namespace openmsx {

SDLOutputSurface::SDLOutputSurface(
		unsigned width, unsigned height, bool fullscreen)
{
	int flags = SDL_SWSURFACE | (fullscreen ? SDL_FULLSCREEN : 0);
	createSurface(width, height, flags);
	memcpy(&format, surface->format, sizeof(SDL_PixelFormat));
	
	data = (char*)surface->pixels;
	pitch = surface->pitch;
}

bool SDLOutputSurface::init()
{
	if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) < 0) {
		return false;
	}
	return true;
}

void SDLOutputSurface::drawFrameBuffer()
{
	// nothing
}

void SDLOutputSurface::finish()
{
	if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
	SDL_Flip(surface);
}

void SDLOutputSurface::takeScreenShot(const std::string& filename)
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

std::auto_ptr<Layer> SDLOutputSurface::createSnowLayer()
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

std::auto_ptr<Layer> SDLOutputSurface::createConsoleLayer(
		MSXMotherBoard& motherboard)
{
	return std::auto_ptr<Layer>(new SDLConsole(motherboard, surface));
}

std::auto_ptr<Layer> SDLOutputSurface::createIconLayer(
		CommandController& commandController,
		Display& display, IconStatus& iconStatus)
{
	return std::auto_ptr<Layer>(new SDLIconLayer(
			commandController, display, iconStatus, surface));
}

} // namespace openmsx
