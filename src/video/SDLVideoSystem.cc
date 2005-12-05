// $Id$

#include "SDLVideoSystem.hh"
#include "SDLRasterizer.hh"
#include "V9990SDLRasterizer.hh"
#include "Display.hh"
#include "SDLOutputSurface.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "InputEventGenerator.hh"
#include "MSXMotherBoard.hh"
#include <cassert>

#include "components.hh"
#ifdef COMPONENT_GL
#include "SDLGLOutputSurface.hh"
#endif

namespace openmsx {

SDLVideoSystem::SDLVideoSystem(MSXMotherBoard& motherboard,
                               RendererFactory::RendererID rendererID)
	: renderSettings(motherboard.getRenderSettings())
	, display(motherboard.getDisplay())
{
	unsigned width, height;
	getWindowSize(width, height);
	bool fullscreen = motherboard.getRenderSettings().
		                      getFullScreen().getValue();
	switch (rendererID) {
	case RendererFactory::SDL:
		screen.reset(new SDLOutputSurface(width, height, fullscreen));
		break;
#ifdef COMPONENT_GL
	case RendererFactory::SDLGL_FB16:
		screen.reset(new SDLGLOutputSurface(width, height, fullscreen,
		             SDLGLOutputSurface::FB_16BPP));
		break;
	case RendererFactory::SDLGL_FB32:
		screen.reset(new SDLGLOutputSurface(width, height, fullscreen,
		             SDLGLOutputSurface::FB_32BPP));
		break;
#endif
	default:
		assert(false);
	}
	motherboard.getInputEventGenerator().reinit();

	snowLayer = screen->createSnowLayer();
	console = screen->createConsoleLayer(motherboard);
	iconLayer = screen->createIconLayer(motherboard.getCommandController(),
	                                    display,
	                                    motherboard.getIconStatus());
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
	unsigned factor = renderSettings.getScaleFactor().getValue();
	width  = 320 * factor;
	height = 240 * factor;
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

void SDLVideoSystem::setWindowTitle(const std::string& title)
{
	screen->setWindowTitle(title);
}

} // namespace openmsx

