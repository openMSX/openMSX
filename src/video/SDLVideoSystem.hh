// $Id$

#ifndef __SDLVIDEOSYSTEM_HH__
#define __SDLVIDEOSYSTEM_HH__

#include "VideoSystem.hh"
#include <SDL.h>


namespace openmsx {

class SDLVideoSystem: public VideoSystem
{
public:
	SDLVideoSystem(SDL_Surface* screen);
	virtual ~SDLVideoSystem();

	// VideoSystem interface:
	virtual bool prepare();
	virtual void flush();
	virtual void takeScreenShot(const string& filename);

private:
	SDL_Surface* screen;
};

} // namespace openmsx

#endif // __SDLVIDEOSYSTEM_HH__
