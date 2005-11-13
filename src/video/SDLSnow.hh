// $Id$

#ifndef SDLSNOW_HH
#define SDLSNOW_HH

#include "Layer.hh"
#include "noncopyable.hh"

struct SDL_Surface;

namespace openmsx {

/** Snow effect for background layer.
  */
template <class Pixel>
class SDLSnow : public Layer, private noncopyable
{
public:
	explicit SDLSnow(SDL_Surface* screen);

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
