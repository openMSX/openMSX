// $Id$

#include "SDLGLVideoSystem.hh"
#include "GLRasterizer.hh"
#include "V9990GLRasterizer.hh"
#include "Reactor.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "SDLGLVisibleSurface.hh"
#include "EventDistributor.hh"
#include "InputEventGenerator.hh"
#include "InputEvents.hh"

using std::string;

namespace openmsx {

SDLGLVideoSystem::SDLGLVideoSystem(Reactor& reactor_)
	: reactor(reactor_)
{
	// Destruct old layers, so resources are freed before new allocations
	// are done.
	// TODO: This has to be done before every video system (re)init,
	//       so move it to a central location.
	Display& display = reactor.getDisplay();

	resize(640, 480);

	console   = screen->createConsoleLayer(reactor);
	snowLayer = screen->createSnowLayer();
	iconLayer = screen->createIconLayer(reactor.getCommandController(),
	                                    display,
	                                    reactor.getIconStatus());
	display.addLayer(*console);
	display.addLayer(*snowLayer);
	display.addLayer(*iconLayer);

	reactor.getEventDistributor().registerEventListener(
		OPENMSX_RESIZE_EVENT, *this);
}

SDLGLVideoSystem::~SDLGLVideoSystem()
{
	reactor.getEventDistributor().unregisterEventListener(
		OPENMSX_RESIZE_EVENT, *this);

	Display& display = reactor.getDisplay();
	display.removeLayer(*iconLayer);
	display.removeLayer(*snowLayer);
	display.removeLayer(*console);
}

Rasterizer* SDLGLVideoSystem::createRasterizer(VDP& vdp)
{
	return new GLRasterizer(reactor.getCommandController(),
	                        vdp, reactor.getDisplay(), *screen);
}

V9990Rasterizer* SDLGLVideoSystem::createV9990Rasterizer(V9990& vdp)
{
	return new V9990GLRasterizer(vdp);
}

// TODO: If we can switch video system at any time (not just frame end),
//       is this polling approach necessary at all?
bool SDLGLVideoSystem::checkSettings()
{
	bool fullScreenTarget = reactor.getDisplay().getRenderSettings().
		getFullScreen().getValue();
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

void SDLGLVideoSystem::resize(unsigned x, unsigned y)
{
	bool fullscreen = reactor.getDisplay().getRenderSettings().
		getFullScreen().getValue();
	screen.reset(new SDLGLVisibleSurface(x, y, fullscreen));
	reactor.getInputEventGenerator().reinit();
}

void SDLGLVideoSystem::signalEvent(const Event& event)
{
	assert(event.getType() == OPENMSX_RESIZE_EVENT);
	const ResizeEvent& resizeEvent = static_cast<const ResizeEvent&>(event);
	resize(resizeEvent.getX(), resizeEvent.getY());
}

} // namespace openmsx
