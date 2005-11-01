// $Id$

#include "SDLVideoSystem.hh"
#include "SDLRasterizer.hh"
#include "V9990SDLRasterizer.hh"
#include "Display.hh"
#include "SDLOutputSurface.hh"
//#include "SDLGLOutputSurface.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "VideoSourceSetting.hh"
#include "InitException.hh"
#include "InputEventGenerator.hh"
#include "MSXMotherBoard.hh"
#include <SDL.h>
#include <cassert>

namespace openmsx {

SDLVideoSystem::SDLVideoSystem(MSXMotherBoard& motherboard)
	: renderSettings(motherboard.getRenderSettings())
	, display(motherboard.getDisplay())
{
	unsigned width, height;
	getWindowSize(width, height);
	bool fullscreen = motherboard.getRenderSettings().
		                      getFullScreen().getValue();
	screen.reset(new SDLOutputSurface(width, height, fullscreen));
	//screen.reset(new SDLGLOutputSurface(width, height, fullscreen));
	motherboard.getInputEventGenerator().reinit();

	snowLayer = screen->createSnowLayer();
	console = screen->createConsoleLayer(motherboard);
	iconLayer = screen->createIconLayer(motherboard.getCommandController(),
	                                    motherboard.getEventDistributor(),
	                                    display);
	display.addLayer(*snowLayer);
	display.addLayer(*console);
	display.addLayer(*iconLayer);
}

SDLVideoSystem::~SDLVideoSystem()
{
	display.removeLayer(*iconLayer);
	display.removeLayer(*console);
	display.removeLayer(*snowLayer);
}

Rasterizer* SDLVideoSystem::createRasterizer(VDP& vdp)
{
	switch (screen->getFormat()->BytesPerPixel) {
	case 2:
		return new SDLRasterizer<Uint16>(vdp, *screen);
	case 4:
		return new SDLRasterizer<Uint32>(vdp, *screen);
	default:
		assert(false);
		return 0;
	}
}

V9990Rasterizer* SDLVideoSystem::createV9990Rasterizer(V9990& vdp)
{
	switch (screen->getFormat()->BytesPerPixel) {
	case 2:
		return new V9990SDLRasterizer<Uint16>(vdp, *screen);
	case 4:
		return new V9990SDLRasterizer<Uint32>(vdp, *screen);
	default:
		assert(false);
		return 0;
	}
}

void SDLVideoSystem::getWindowSize(unsigned& width, unsigned& height)
{
	// TODO refactor
	if (renderSettings.getScaler().getValue() == SCALER_LOW) {
		width = 320;
		height = 240;
	} else if ((renderSettings.getScaler().getValue() == SCALER_HQ3X) ||
	           (renderSettings.getScaler().getValue() == SCALER_HQ3XLITE) ||
	           (renderSettings.getScaler().getValue() == SCALER_SIMPLE3X) ||
	           (renderSettings.getScaler().getValue() == SCALER_RGBTRIPLET3X )) {
		width = 960;
		height = 720;
	} else {
		width = 640;
		height = 480;
	}
}

// TODO: If we can switch video system at any time (not just frame end),
//       is this polling approach necessary at all?
// TODO: Code is exactly the same as SDLGLVideoSystem::checkSettings.
bool SDLVideoSystem::checkSettings()
{
	// Check resolution
	unsigned width, height;
	getWindowSize(width, height);
	if ((width  != screen->getWidth()) ||
	    (height != screen->getHeight())) {
		return false;
	}

	// Check fullscreen
	const bool fullScreenTarget =
		renderSettings.getFullScreen().getValue();
	return screen->setFullScreen(fullScreenTarget);
}

bool SDLVideoSystem::prepare()
{
	return screen->init();
}

void SDLVideoSystem::flush()
{
	screen->finish();
}

void SDLVideoSystem::takeScreenShot(const std::string& filename)
{
	screen->takeScreenShot(filename);
}

} // namespace openmsx

