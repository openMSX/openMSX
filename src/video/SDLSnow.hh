// $Id$

#ifndef SDLSNOW_HH
#define SDLSNOW_HH

#include "Layer.hh"

struct SDL_Surface;

namespace openmsx {

class Display;

/** Snow effect for background layer.
  */
template <class Pixel>
class SDLSnow: public Layer
{
public:
	SDLSnow(Display& display, SDL_Surface* screen);

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

#endif
