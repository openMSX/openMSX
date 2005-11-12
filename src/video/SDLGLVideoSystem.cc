// $Id$

#include "SDLGLVideoSystem.hh"
#include "GLRasterizer.hh"
#include "V9990GLRasterizer.hh"
#include "MSXMotherBoard.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "SDLGLOutputSurface.hh"
#include "EventDistributor.hh"
#include "InputEventGenerator.hh"
#include "InputEvents.hh"

using std::string;

namespace openmsx {

SDLGLVideoSystem::SDLGLVideoSystem(MSXMotherBoard& motherboard_)
	: motherboard(motherboard_)
{
	// Destruct old layers, so resources are freed before new allocations
	// are done.
	// TODO: This has to be done before every video system (re)init,
	//       so move it to a central location.
	Display& display = motherboard.getDisplay();

	resize(640, 480);

	console   = screen->createConsoleLayer(motherboard);
	snowLayer = screen->createSnowLayer();
	iconLayer = screen->createIconLayer(motherboard.getCommandController(),
	                                    motherboard.getEventDistributor(),
	                                    display);
	display.addLayer(*console);
	display.addLayer(*snowLayer);
	display.addLayer(*iconLayer);

	motherboard.getEventDistributor().registerEventListener(
		OPENMSX_RESIZE_EVENT, *this, EventDistributor::DETACHED);
}

SDLGLVideoSystem::~SDLGLVideoSystem()
{
	motherboard.getEventDistributor().unregisterEventListener(
		OPENMSX_RESIZE_EVENT, *this, EventDistributor::DETACHED);

	Display& display = motherboard.getDisplay();
	display.removeLayer(*iconLayer);
	display.removeLayer(*snowLayer);
	display.removeLayer(*console);
}

Rasterizer* SDLGLVideoSystem::createRasterizer(VDP& vdp)
{
	return new GLRasterizer(motherboard.getCommandController(),
	                        motherboard.getRenderSettings(),
	                        motherboard.getDisplay(), vdp);
}

V9990Rasterizer* SDLGLVideoSystem::createV9990Rasterizer(V9990& vdp)
{
	return new V9990GLRasterizer(vdp);
}

// TODO: If we can switch video system at any time (not just frame end),
//       is this polling approach necessary at all?
bool SDLGLVideoSystem::checkSettings()
{
	bool fullScreenTarget = motherboard.getRenderSettings().
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
	bool fullscreen = motherboard.getRenderSettings().
		                      getFullScreen().getValue();
	screen.reset(new SDLGLOutputSurface(x, y, fullscreen));
	motherboard.getInputEventGenerator().reinit();
}

void SDLGLVideoSystem::signalEvent(const Event& event)
{
	assert(event.getType() == OPENMSX_RESIZE_EVENT);
	const ResizeEvent& resizeEvent = static_cast<const ResizeEvent&>(event);
	resize(resizeEvent.getX(), resizeEvent.getY());
}

} // namespace openmsx
