// $Id$

#ifndef __SDLGLVIDEOSYSTEM_HH__
#define __SDLGLVIDEOSYSTEM_HH__

#include "VideoSystem.hh"

struct SDL_Surface;


namespace openmsx {

class VDP;
class Renderer;

class SDLGLVideoSystem: public VideoSystem
{
public:
	/** Activates this video system.
	  * Updates Display::INSTANCE as well.
	  * @throw InitException If initialisation fails.
	  */
	SDLGLVideoSystem(VDP* vdp);

	/** Deactivates this video system.
	  * Called as a result of Display::INSTANCE.reset().
	  */
	virtual ~SDLGLVideoSystem();

	// VideoSystem interface:
	virtual bool checkSettings();
	virtual void flush();
	virtual void takeScreenShot(const string& filename);

	/** TODO: Only here for backwards compatibility. */
	Renderer* renderer;

private:
	SDL_Surface* screen;
};

} // namespace openmsx

#endif // __SDLGLVIDEOSYSTEM_HH__
