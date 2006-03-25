// $Id$

#include "SDLVideoSystem.hh"
#include "SDLRasterizer.hh"
#include "V9990.hh"
#include "V9990SDLRasterizer.hh"
#include "Display.hh"
#include "SDLVisibleSurface.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "InputEventGenerator.hh"
#include "Reactor.hh"
#include "FBPostProcessor.hh"
#include "VideoSourceSetting.hh"
#include "MSXMotherBoard.hh"
#include <cassert>

#include "components.hh"
#ifdef COMPONENT_GL
#include "SDLGLVisibleSurface.hh"
#endif

namespace openmsx {

SDLVideoSystem::SDLVideoSystem(Reactor& reactor,
                               RendererFactory::RendererID rendererID)
	: renderSettings(reactor.getDisplay().getRenderSettings())
	, display(reactor.getDisplay())
{
	unsigned width, height;
	getWindowSize(width, height);
	bool fullscreen = renderSettings.getFullScreen().getValue();
	switch (rendererID) {
	case RendererFactory::SDL:
		screen.reset(new SDLVisibleSurface(width, height, fullscreen));
		break;
#ifdef COMPONENT_GL
	case RendererFactory::SDLGL_FB16:
		screen.reset(new SDLGLVisibleSurface(width, height, fullscreen,
		             SDLGLVisibleSurface::FB_16BPP));
		break;
	case RendererFactory::SDLGL_FB32:
		screen.reset(new SDLGLVisibleSurface(width, height, fullscreen,
		             SDLGLVisibleSurface::FB_32BPP));
		break;
#endif
	default:
		assert(false);
	}
	reactor.getInputEventGenerator().reinit();

	snowLayer = screen->createSnowLayer();
	console = screen->createConsoleLayer(reactor);
	iconLayer = screen->createIconLayer(reactor.getCommandController(),
	                                    display,
	                                    reactor.getIconStatus());
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
		return new SDLRasterizer<Uint16>(
			vdp, display, *screen,
			std::auto_ptr<PostProcessor>(new FBPostProcessor<Uint16>(
				vdp.getMotherBoard().getCommandController(),
				display, *screen, VIDEO_MSX, 640, 240
				))
			);
	case 4:
		return new SDLRasterizer<Uint32>(
			vdp, display, *screen,
			std::auto_ptr<PostProcessor>(new FBPostProcessor<Uint32>(
				vdp.getMotherBoard().getCommandController(),
				display, *screen, VIDEO_MSX, 640, 240
				))
			);
	default:
		assert(false);
		return 0;
	}
}

V9990Rasterizer* SDLVideoSystem::createV9990Rasterizer(V9990& vdp)
{
	switch (screen->getFormat()->BytesPerPixel) {
	case 2:
		return new V9990SDLRasterizer<Uint16>(
			vdp, display, *screen,
			std::auto_ptr<PostProcessor>(new FBPostProcessor<Uint32>(
				vdp.getMotherBoard().getCommandController(),
				display, *screen, VIDEO_GFX9000, 1280, 240
				))
			);
	case 4:
		return new V9990SDLRasterizer<Uint32>(
			vdp, display, *screen,
			std::auto_ptr<PostProcessor>(new FBPostProcessor<Uint32>(
				vdp.getMotherBoard().getCommandController(),
				display, *screen, VIDEO_GFX9000, 1280, 240
				))
			);
	default:
		assert(false);
		return 0;
	}
}

void SDLVideoSystem::getWindowSize(unsigned& width, unsigned& height)
{
	unsigned factor = renderSettings.getScaleFactor().getValue();
	if (factor > 3) factor = 3; // fallback
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

