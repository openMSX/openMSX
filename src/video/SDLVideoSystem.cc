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

bool SDLVideoSystem::prepare()
{
	if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) {
		return false;
	}
	return true;
}

void SDLVideoSystem::flush()
{
	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	SDL_Flip(screen);
}

void SDLVideoSystem::takeScreenShot(const string& filename)
{
	if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) {
		throw CommandException("Failed to lock surface.");
	}
	try {
		ScreenShotSaver::save(screen, filename);
		if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	} catch (CommandException& e) {
		if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		throw;
	}
}

} // namespace openmsx

