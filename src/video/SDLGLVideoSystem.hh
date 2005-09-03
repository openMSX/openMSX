// $Id$

#ifndef SDLGLVIDEOSYSTEM_HH
#define SDLGLVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include "EventListener.hh"
#include <string>

struct SDL_Surface;

namespace openmsx {

class MSXMotherBoard;
class VDP;
class V9990;
class Rasterizer;

class SDLGLVideoSystem : public VideoSystem, private EventListener
{
public:
	/** Activates this video system.
	  * @throw InitException If initialisation fails.
	  */
	SDLGLVideoSystem(MSXMotherBoard& motherboard);

	/** Deactivates this video system.
	  */
	virtual ~SDLGLVideoSystem();

	// VideoSystem interface:
	virtual Rasterizer* createRasterizer(VDP& vdp);
	virtual V9990Rasterizer* createV9990Rasterizer(V9990& vdp);
	virtual bool checkSettings();
	virtual void flush();
	virtual void takeScreenShot(const std::string& filename);

	// EventListener
	void signalEvent(const Event& event);

private:
	void resize(unsigned x, unsigned y);

	MSXMotherBoard& motherboard;
	SDL_Surface* screen;
};

} // namespace openmsx

#endif
