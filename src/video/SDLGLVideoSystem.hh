// $Id$

#ifndef SDLGLVIDEOSYSTEM_HH
#define SDLGLVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include "EventListener.hh"
#include <string>

struct SDL_Surface;

namespace openmsx {

class VDP;
class V9990;
class Rasterizer;

class SDLGLVideoSystem : public VideoSystem, private EventListener
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
	virtual Rasterizer* createRasterizer(VDP& vdp);
	virtual V9990Rasterizer* createV9990Rasterizer(V9990& vdp);
	virtual bool checkSettings();
	virtual void flush();
	virtual void takeScreenShot(const std::string& filename);

	// EventListener
	bool signalEvent(const Event& event);

private:
	void resize(unsigned x, unsigned y);

	SDL_Surface* screen;
};

} // namespace openmsx

#endif
