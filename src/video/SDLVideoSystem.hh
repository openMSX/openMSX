// $Id$

#ifndef SDLVIDEOSYSTEM_HH
#define SDLVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include "EventListener.hh"
#include "Observer.hh"
#include "noncopyable.hh"
#include <memory>

#include "components.hh"

namespace openmsx {

class Reactor;
class CommandConsole;
class Display;
class RenderSettings;
class VisibleSurface;
class Layer;
class Setting;

class SDLVideoSystem : public VideoSystem, private EventListener,
                       private Observer<Setting>, private noncopyable
{
public:
	/** Activates this video system.
	  * @throw InitException If initialisation fails.
	  */
	explicit SDLVideoSystem(Reactor& reactor, CommandConsole& console);

	/** Deactivates this video system.
	  */
	virtual ~SDLVideoSystem();

	// VideoSystem interface:
	virtual Rasterizer* createRasterizer(VDP& vdp);
	virtual V9990Rasterizer* createV9990Rasterizer(V9990& vdp);
#if COMPONENT_LASERDISC
	virtual LDRasterizer* createLDRasterizer(LaserdiscPlayer& ld);
#endif
	virtual bool checkSettings();
	virtual void flush();
	virtual void takeScreenShot(const std::string& filename, bool withOsd);
	virtual void setWindowTitle(const std::string& title);
	virtual OutputSurface* getOutputSurface();

private:
	// EventListener
	virtual int signalEvent(const shared_ptr<const Event>& event);
	// Observer
	void update(const Setting& subject);

	void getWindowSize(unsigned& width, unsigned& height);
	void resize();

	Reactor& reactor;
	Display& display;
	RenderSettings& renderSettings;
	std::auto_ptr<VisibleSurface> screen;
	std::auto_ptr<Layer> consoleLayer;
	std::auto_ptr<Layer> snowLayer;
	std::auto_ptr<Layer> iconLayer;
	std::auto_ptr<Layer> osdGuiLayer;
};

} // namespace openmsx

#endif
