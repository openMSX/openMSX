// $Id$

#include "SDLVideoSystem.hh"
#include "ScreenShotSaver.hh"


namespace openmsx {

// Dimensions of screen.
static const int WIDTH = 640;
static const int HEIGHT = 480;

SDLVideoSystem::SDLVideoSystem(SDL_Surface* screen)
	: screen(screen)
{
}

SDLVideoSystem::~SDLVideoSystem()
{
}

void SDLVideoSystem::flush()
{
	SDL_Flip(screen);
}

void SDLVideoSystem::takeScreenShot(const string& filename)
{
	if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) {
		return;
	}
	try {
		ScreenShotSaver::save(screen, filename);
		if (SDL_MUSTLOCK(screen)) {
			SDL_UnlockSurface(screen);
		}
	} catch (CommandException& e) {
		// TODO: Make a SDLSurfaceLocker class to get rid of this code
		//       duplication.
		if (SDL_MUSTLOCK(screen)) {
			SDL_UnlockSurface(screen);
		}
		throw;
	}
}

} // namespace openmsx

