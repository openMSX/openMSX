// $Id$

#include "SDLVisibleSurface.hh"
#include "ScreenShotSaver.hh"
#include "CommandException.hh"
#include "SDLSnow.hh"
#include "SDLConsole.hh"
#include "IconLayer.hh"
#include <cstring>
#include <cassert>

namespace openmsx {

SDLVisibleSurface::SDLVisibleSurface(
		unsigned width, unsigned height, bool fullscreen)
{
	int flags = SDL_SWSURFACE | (fullscreen ? SDL_FULLSCREEN : 0);
	createSurface(width, height, flags);
	SDL_Surface* surface = getSDLSurface();
	memcpy(&getSDLFormat(), surface->format, sizeof(SDL_PixelFormat));

	setBufferPtr(static_cast<char*>(surface->pixels), surface->pitch);
}

void SDLVisibleSurface::init()
{
	// nothing
}

void SDLVisibleSurface::drawFrameBuffer()
{
	// nothing
}

void SDLVisibleSurface::finish()
{
	unlock();
	SDL_Flip(getSDLSurface());
}

void SDLVisibleSurface::takeScreenShot(const std::string& filename)
{
	lock();
	try {
		ScreenShotSaver::save(getSDLSurface(), filename);
	} catch (CommandException& e) {
		throw;
	}
}

std::auto_ptr<Layer> SDLVisibleSurface::createSnowLayer()
{
	switch (getSDLFormat().BytesPerPixel) {
	case 2:
		return std::auto_ptr<Layer>(new SDLSnow<Uint16>(*this));
	case 4:
		return std::auto_ptr<Layer>(new SDLSnow<Uint32>(*this));
	default:
		assert(false);
		return std::auto_ptr<Layer>(); // avoid warning
	}
}

std::auto_ptr<Layer> SDLVisibleSurface::createConsoleLayer(
		Reactor& reactor)
{
	return std::auto_ptr<Layer>(new SDLConsole(reactor, *this));
}

std::auto_ptr<Layer> SDLVisibleSurface::createIconLayer(
		CommandController& commandController,
		Display& display, IconStatus& iconStatus)
{
	return std::auto_ptr<Layer>(new SDLIconLayer(
			commandController, display, iconStatus, *this));
}

} // namespace openmsx
