// $Id$

#ifndef __SDLSNOW_HH__
#define __SDLSNOW_HH__

#include "Layer.hh"
#include <SDL.h>

namespace openmsx {

/** Snow effect for background layer.
  */
template <class Pixel>
class SDLSnow: public Layer
{
public:
	SDLSnow(SDL_Surface* screen);
	virtual ~SDLSnow();

	// Layer interface:
	virtual void paint();
	virtual const std::string& getName();

private:
	SDL_Surface* screen;

	/** Gray values for noise.
	  */
	Pixel gray[256];
};

} // namespace openmsx

#endif // __SDLSNOW_HH__
