// $Id$

#ifndef __SDLGLVIDEOSYSTEM_HH__
#define __SDLGLVIDEOSYSTEM_HH__

#include "VideoSystem.hh"

struct SDL_Surface;


namespace openmsx {

class VDP;
class Rasterizer;

class SDLGLVideoSystem: public VideoSystem
{
public:
	/** Activates this video system.
	  * Updates Display::INSTANCE as well.
	  * @throw InitException If initialisation fails.
	  */
	SDLGLVideoSystem();

	/** Deactivates this video system.
	  * Called as a result of Display::INSTANCE.reset().
	  */
	virtual ~SDLGLVideoSystem();

	// VideoSystem interface:
	virtual Rasterizer* createRasterizer(VDP* vdp);
	virtual V9990Rasterizer* createV9990Rasterizer(V9990* vdp);
	virtual bool checkSettings();
	virtual void flush();
	virtual void takeScreenShot(const string& filename);

private:
	SDL_Surface* screen;
};

} // namespace openmsx

#endif // __SDLGLVIDEOSYSTEM_HH__
