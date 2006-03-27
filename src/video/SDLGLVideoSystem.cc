// $Id$

#include "SDLGLVideoSystem.hh"
#include "GLRasterizer.hh"
#include "GL2Rasterizer.hh"
#include "SDLRasterizer.hh"
#include "V9990.hh"
#include "V9990GLRasterizer.hh"
#include "V9990SDLRasterizer.hh"
#include "Reactor.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "SDLGLVisibleSurface.hh"
#include "EventDistributor.hh"
#include "InputEventGenerator.hh"
#include "InputEvents.hh"
#include "GLPostProcessor.hh"
#include "VideoSourceSetting.hh"

using std::string;

namespace openmsx {

SDLGLVideoSystem::SDLGLVideoSystem(Reactor& reactor_)
	: reactor(reactor_)
	, display(reactor.getDisplay())
	, renderSettings(reactor.getDisplay().getRenderSettings())
{
	// Destruct old layers, so resources are freed before new allocations
	// are done.
	// TODO: This has to be done before every video system (re)init,
	//       so move it to a central location.
	resize();

	console   = screen->createConsoleLayer(reactor);
	snowLayer = screen->createSnowLayer();
	iconLayer = screen->createIconLayer(reactor.getCommandController(),
	                                    display,
	                                    reactor.getIconStatus());
	display.addLayer(*console);
	display.addLayer(*snowLayer);
	display.addLayer(*iconLayer);

	renderSettings.getScaleFactor().attach(*this);

	reactor.getEventDistributor().registerEventListener(
		OPENMSX_RESIZE_EVENT, *this);
}

SDLGLVideoSystem::~SDLGLVideoSystem()
{
	reactor.getEventDistributor().unregisterEventListener(
		OPENMSX_RESIZE_EVENT, *this);

	renderSettings.getScaleFactor().detach(*this);

	display.removeLayer(*iconLayer);
	display.removeLayer(*snowLayer);
	display.removeLayer(*console);
}

Rasterizer* SDLGLVideoSystem::createRasterizer(VDP& vdp)
{
	switch (renderSettings.getRenderer().getValue()) {
	case RendererFactory::SDLGL:
		return new GLRasterizer(
			reactor.getCommandController(), vdp, display, *screen
			);
	case RendererFactory::SDLGL2:
		return new GL2Rasterizer(
			reactor.getCommandController(), vdp, display, *screen
			);
	case RendererFactory::SDLGL_PP:
		return new SDLRasterizer<unsigned>(
			vdp, display, *screen,
			std::auto_ptr<PostProcessor>(new GLPostProcessor(
				reactor.getCommandController(),
				display, *screen, VIDEO_MSX, 640, 240
				))
			);
	default:
		assert(false);
		return NULL;
	}
}

V9990Rasterizer* SDLGLVideoSystem::createV9990Rasterizer(V9990& vdp)
{
	switch (renderSettings.getRenderer().getValue()) {
	case RendererFactory::SDLGL:
	case RendererFactory::SDLGL2:
		return new V9990GLRasterizer(vdp);
	case RendererFactory::SDLGL_PP:
		return new V9990SDLRasterizer<unsigned>(
			vdp, display, *screen,
			std::auto_ptr<PostProcessor>(new GLPostProcessor(
				reactor.getCommandController(),
				display, *screen, VIDEO_GFX9000, 1280, 240
				))
			);
	default:
		assert(false);
		return NULL;
	}
}

// TODO: Modified copy-paste from SDLVideoSystem.
void SDLGLVideoSystem::getWindowSize(unsigned& width, unsigned& height)
{
	unsigned factor = renderSettings.getScaleFactor().getValue();
	if (renderSettings.getRenderer().getValue() != RendererFactory::SDLGL_PP) {
		// TODO: Other GL-based renderers cannot deal with different scale
		//       factors yet.
		factor = 2;
	}
	width  = 320 * factor;
	height = 240 * factor;
}

// TODO: If we can switch video system at any time (not just frame end),
//       is this polling approach necessary at all?
bool SDLGLVideoSystem::checkSettings()
{
	// Check resolution.
	unsigned width, height;
	getWindowSize(width, height);
	if (width != screen->getWidth() || height != screen->getHeight()) {
		return false;
	}

	bool fullScreenTarget = renderSettings.getFullScreen().getValue();
	return screen->setFullScreen(fullScreenTarget);
}

void SDLGLVideoSystem::flush()
{
	SDL_GL_SwapBuffers();
}

void SDLGLVideoSystem::takeScreenShot(const string& filename)
{
	screen->takeScreenShot(filename);
}

void SDLGLVideoSystem::setWindowTitle(const std::string& title)
{
	screen->setWindowTitle(title);
}

void SDLGLVideoSystem::resize()
{
	unsigned width, height;
	getWindowSize(width, height);
	bool fullscreen = renderSettings.getFullScreen().getValue();
	// Destruct existing output surface before creating a new one.
	screen.reset();
	screen.reset(new SDLGLVisibleSurface(width, height, fullscreen));
	reactor.getInputEventGenerator().reinit();
}

void SDLGLVideoSystem::update(const Setting& subject)
{
	if (&subject == &renderSettings.getScaleFactor()) {
		// TODO: This is done via checkSettings instead,
		//       but is that still needed?
		//resize();
	} else {
		assert(false);
	}
}

void SDLGLVideoSystem::signalEvent(const Event& event)
{
	// TODO: Currently window size depends only on scale factor.
	//       Maybe in the future it will be handled differently.
	assert(event.getType() == OPENMSX_RESIZE_EVENT);
	//const ResizeEvent& resizeEvent = static_cast<const ResizeEvent&>(event);
	//resize(resizeEvent.getX(), resizeEvent.getY());
	resize();
}

} // namespace openmsx
