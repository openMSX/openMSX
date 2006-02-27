// $Id$

#ifndef SDLGLVIDEOSYSTEM_HH
#define SDLGLVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include "EventListener.hh"
#include "noncopyable.hh"
#include <string>
#include <memory>

namespace openmsx {

class Reactor;
class VisibleSurface;
class Layer;

class SDLGLVideoSystem : public VideoSystem, private EventListener,
                         private noncopyable
{
public:
	/** Activates this video system.
	  * @throw InitException If initialisation fails.
	  */
	explicit SDLGLVideoSystem(Reactor& reactor);

	/** Deactivates this video system.
	  */
	virtual ~SDLGLVideoSystem();

	// VideoSystem interface:
	virtual Rasterizer* createRasterizer(VDP& vdp);
	virtual V9990Rasterizer* createV9990Rasterizer(V9990& vdp);
	virtual bool checkSettings();
	virtual void flush();
	virtual void takeScreenShot(const std::string& filename);
	virtual void setWindowTitle(const std::string& title);

	// EventListener
	void signalEvent(const Event& event);

private:
	void resize(unsigned x, unsigned y);

	Reactor& reactor;
	std::auto_ptr<VisibleSurface> screen;
	std::auto_ptr<Layer> console;
	std::auto_ptr<Layer> snowLayer;
	std::auto_ptr<Layer> iconLayer;
};

} // namespace openmsx

#endif
