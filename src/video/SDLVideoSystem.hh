// $Id$

#ifndef SDLVIDEOSYSTEM_HH
#define SDLVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include "RendererFactory.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class Reactor;
class Display;
class RenderSettings;
class VisibleSurface;
class Layer;

class SDLVideoSystem : public VideoSystem, private noncopyable
{
public:
	SDLVideoSystem(Reactor& reactor,
	               RendererFactory::RendererID rendererID);
	virtual ~SDLVideoSystem();

	// VideoSystem interface:
	virtual Rasterizer* createRasterizer(VDP& vdp);
	virtual V9990Rasterizer* createV9990Rasterizer(V9990& vdp);
	virtual bool checkSettings();
	virtual bool prepare();
	virtual void flush();
	virtual void takeScreenShot(const std::string& filename);
	virtual void setWindowTitle(const std::string& title);

private:
	void getWindowSize(unsigned& width, unsigned& height);

	Reactor& reactor;
	Display& display;
	RenderSettings& renderSettings;
	std::auto_ptr<VisibleSurface> screen;
	std::auto_ptr<Layer> console;
	std::auto_ptr<Layer> snowLayer;
	std::auto_ptr<Layer> iconLayer;
};

} // namespace openmsx

#endif
