// $Id$

#include "SDLVideoSystem.hh"
#include "SDLVisibleSurface.hh"
#include "SDLRasterizer.hh"
#include "V9990SDLRasterizer.hh"
#include "FBPostProcessor.hh"
#include "Reactor.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "EventDistributor.hh"
#include "InputEventGenerator.hh"
#include "VDP.hh"
#include "V9990.hh"
#include "build-info.hh"
#include "openmsx.hh"
#include <memory>
#include <cassert>

#ifdef _WIN32
#include "AltSpaceSuppressor.hh"
#include "sdlwin32.hh"
#endif
#include "components.hh"
#ifdef COMPONENT_GL
#include "SDLGLVisibleSurface.hh"
#include "GLPostProcessor.hh"
#endif

namespace openmsx {

SDLVideoSystem::SDLVideoSystem(Reactor& reactor)
	: reactor(reactor)
	, display(reactor.getDisplay())
	, renderSettings(reactor.getDisplay().getRenderSettings())
{
	resize();

	console = screen->createConsoleLayer(reactor);
	snowLayer = screen->createSnowLayer(display);
	osdGuiLayer = screen->createOSDGUILayer(display.getOSDGUI());
	display.addLayer(*console);
	display.addLayer(*snowLayer);
	display.addLayer(*osdGuiLayer);

	renderSettings.getScaleFactor().attach(*this);

	reactor.getEventDistributor().registerEventListener(
		OPENMSX_RESIZE_EVENT, *this);

#ifdef _WIN32
	HWND hWnd = getSDLWindowHandle();
	assert(hWnd);
	AltSpaceSuppressor::Start(hWnd);
#endif
}

SDLVideoSystem::~SDLVideoSystem()
{
#ifdef _WIN32
	// This needs to be done while the SDL window handle is still valid
	assert(getSDLWindowHandle());
	AltSpaceSuppressor::Stop();
#endif

	reactor.getEventDistributor().unregisterEventListener(
		OPENMSX_RESIZE_EVENT, *this);

	renderSettings.getScaleFactor().detach(*this);

	display.removeLayer(*osdGuiLayer);
	display.removeLayer(*snowLayer);
	display.removeLayer(*console);
}

Rasterizer* SDLVideoSystem::createRasterizer(VDP& vdp)
{
	MSXMotherBoard& motherBoard = vdp.getMotherBoard();
	switch (renderSettings.getRenderer().getValue()) {
	case RendererFactory::SDL:
	case RendererFactory::SDLGL_FB16:
	case RendererFactory::SDLGL_FB32:
		switch (screen->getSDLFormat().BytesPerPixel) {
#if HAVE_16BPP
		case 2:
			return new SDLRasterizer<word>(
				vdp, display, *screen,
				std::auto_ptr<PostProcessor>(new FBPostProcessor<word>(
					motherBoard, display, *screen,
					VIDEO_MSX, 640, 240)));
#endif
#if HAVE_32BPP
		case 4:
			return new SDLRasterizer<unsigned>(
				vdp, display, *screen,
				std::auto_ptr<PostProcessor>(new FBPostProcessor<unsigned>(
					motherBoard, display, *screen,
					VIDEO_MSX, 640, 240)));
#endif
		default:
			assert(false);
			return NULL;
		}
#ifdef COMPONENT_GL
	case RendererFactory::SDLGL_PP:
		return new SDLRasterizer<unsigned>(
			vdp, display, *screen,
			std::auto_ptr<PostProcessor>(new GLPostProcessor(
				motherBoard, display, *screen,
				VIDEO_MSX, 640, 240)));
#endif
	default:
		assert(false);
		return NULL;
	}
}

V9990Rasterizer* SDLVideoSystem::createV9990Rasterizer(V9990& vdp)
{
	MSXMotherBoard& motherBoard = vdp.getMotherBoard();
	switch (renderSettings.getRenderer().getValue()) {
	case RendererFactory::SDL:
	case RendererFactory::SDLGL_FB16:
	case RendererFactory::SDLGL_FB32:
		switch (screen->getSDLFormat().BytesPerPixel) {
#if HAVE_16BPP
		case 2:
			return new V9990SDLRasterizer<word>(
				vdp, display, *screen,
				std::auto_ptr<PostProcessor>(new FBPostProcessor<word>(
					motherBoard, display, *screen,
					VIDEO_GFX9000, 1280, 240)));
#endif
#if HAVE_32BPP
		case 4:
			return new V9990SDLRasterizer<unsigned>(
				vdp, display, *screen,
				std::auto_ptr<PostProcessor>(new FBPostProcessor<unsigned>(
					motherBoard, display, *screen,
					VIDEO_GFX9000, 1280, 240)));
#endif
		default:
			assert(false);
			return NULL;
		}
#ifdef COMPONENT_GL
	case RendererFactory::SDLGL_PP:
		return new V9990SDLRasterizer<unsigned>(
			vdp, display, *screen,
			std::auto_ptr<PostProcessor>(new GLPostProcessor(
				motherBoard, display, *screen,
				VIDEO_GFX9000, 1280, 240)));
#endif
	default:
		assert(false);
		return NULL;
	}
}

void SDLVideoSystem::getWindowSize(unsigned& width, unsigned& height)
{
	unsigned factor = renderSettings.getScaleFactor().getValue();
	switch (renderSettings.getRenderer().getValue()) {
	case RendererFactory::SDL:
	case RendererFactory::SDLGL_FB16:
	case RendererFactory::SDLGL_FB32:
		// We don't have 4x software scalers yet.
		if (factor > 3) factor = 3;
		break;
	case RendererFactory::SDLGL_PP:
		// All scale factors are supported.
		break;
	case RendererFactory::DUMMY:
		factor = 0;
		break;
	default:
		assert(false);
	}
	width  = 320 * factor;
	height = 240 * factor;
}

// TODO: If we can switch video system at any time (not just frame end),
//       is this polling approach necessary at all?
bool SDLVideoSystem::checkSettings()
{
	// Check resolution.
	unsigned width, height;
	getWindowSize(width, height);
	if (width != screen->getWidth() || height != screen->getHeight()) {
		return false;
	}

	// Check fullscreen.
	const bool fullScreenTarget = renderSettings.getFullScreen().getValue();
	return screen->setFullScreen(fullScreenTarget);
}

void SDLVideoSystem::flush()
{
	screen->finish();
}

void SDLVideoSystem::takeScreenShot(const std::string& filename, bool withOsd)
{
	if (withOsd) {
		// we can directly save current content as screenshot
		screen->saveScreenshot(filename);
	} else {
		// we first need to re-render to an off-screen surface
		// with OSD layers disabled
		ScopedLayerHider hideConsole(*console);
		ScopedLayerHider hideOsd(*osdGuiLayer);
		std::auto_ptr<OutputSurface> surf = screen->createOffScreenSurface();
		display.repaint(*surf);
		surf->saveScreenshot(filename);
	}
}

void SDLVideoSystem::setWindowTitle(const std::string& title)
{
	screen->setWindowTitle(title);
}

OutputSurface* SDLVideoSystem::getOutputSurface()
{
	return screen.get();
}

void SDLVideoSystem::resize()
{
	EventDistributor& eventDistributor = reactor.getEventDistributor();

	unsigned width, height;
	getWindowSize(width, height);
	const bool fullscreen = renderSettings.getFullScreen().getValue();
	// Destruct existing output surface before creating a new one.
	screen.reset();

	switch (renderSettings.getRenderer().getValue()) {
	case RendererFactory::SDL:
		screen.reset(new SDLVisibleSurface(width, height, fullscreen,
					renderSettings, eventDistributor));
		break;
#ifdef COMPONENT_GL
	case RendererFactory::SDLGL_PP:
		screen.reset(new SDLGLVisibleSurface(width, height, fullscreen,
					renderSettings, eventDistributor));
		break;
	case RendererFactory::SDLGL_FB16:
		screen.reset(new SDLGLVisibleSurface(width, height, fullscreen,
					renderSettings, eventDistributor,
					SDLGLVisibleSurface::FB_16BPP));
		break;
	case RendererFactory::SDLGL_FB32:
		screen.reset(new SDLGLVisibleSurface(width, height, fullscreen,
					renderSettings, eventDistributor,
					SDLGLVisibleSurface::FB_32BPP));
		break;
#endif
	default:
		assert(false);
	}
	reactor.getInputEventGenerator().reinit();
}

void SDLVideoSystem::update(const Setting& subject)
{
	if (&subject == &renderSettings.getScaleFactor()) {
		// TODO: This is done via checkSettings instead,
		//       but is that still needed?
		//resize();
	} else {
		assert(false);
	}
}

bool SDLVideoSystem::signalEvent(shared_ptr<const Event> /*event*/)
{
	// TODO: Currently window size depends only on scale factor.
	//       Maybe in the future it will be handled differently.
	//const ResizeEvent& resizeEvent = checked_cast<const ResizeEvent&>(event);
	//resize(resizeEvent.getX(), resizeEvent.getY());
	//resize();
	return true;
}

} // namespace openmsx
