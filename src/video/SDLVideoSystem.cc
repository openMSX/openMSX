// $Id$

#include "SDLVideoSystem.hh"
#include "SDLRasterizer.hh"
#include "V9990SDLRasterizer.hh"
#include "SDLSnow.hh"
#include "SDLConsole.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "SDLUtil.hh"
#include "CommandConsole.hh"
#include "InitException.hh"
#include "ScreenShotSaver.hh"
#include "IconLayer.hh"
#include <SDL.h>
#include <cassert>

using std::string;

namespace openmsx {

SDLVideoSystem::SDLVideoSystem(RendererFactory::RendererID id)
	: id(id)
{
	// Destruct old layers, so resources are freed before new allocations
	// are done.
	Display::INSTANCE.reset();

	assert(id == RendererFactory::SDLHI || id == RendererFactory::SDLLO);
	screen = openSDLVideo(
		(id == RendererFactory::SDLLO) ? 320 : 640, // width
		(id == RendererFactory::SDLLO) ? 240 : 480, // height
		SDL_SWSURFACE
		);

	Display* display = new Display(std::auto_ptr<VideoSystem>(this));
	Display::INSTANCE.reset(display);
	switch (screen->format->BytesPerPixel) {
	case 2:
		new SDLSnow<Uint16>(screen);
		break;
	case 4:
		new SDLSnow<Uint32>(screen);
		break;
	default:
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		throw InitException("unsupported colour depth");
	}
	new SDLConsole(CommandConsole::instance(), screen);

	Layer* iconLayer = new SDLIconLayer(screen);
	Display::INSTANCE->addLayer(iconLayer);
}

SDLVideoSystem::~SDLVideoSystem()
{
	closeSDLVideo(screen);
}

Rasterizer* SDLVideoSystem::createRasterizer(VDP* vdp)
{
	switch (screen->format->BytesPerPixel) {
	case 2:
		if (id == RendererFactory::SDLLO) {
			return new SDLRasterizer<Uint16, Renderer::ZOOM_256>(vdp, screen);
		} else {
			return new SDLRasterizer<Uint16, Renderer::ZOOM_REAL>(vdp, screen);
		}
	case 4:
		if (id == RendererFactory::SDLLO) {
			return new SDLRasterizer<Uint32, Renderer::ZOOM_256>(vdp, screen);
		} else {
			return new SDLRasterizer<Uint32, Renderer::ZOOM_REAL>(vdp, screen);
		}
	default:
		assert(false);
		return 0;
	}
}

V9990Rasterizer* SDLVideoSystem::createV9990Rasterizer(V9990* vdp)
{
	switch (screen->format->BytesPerPixel) {
	case 2:
		if (id == RendererFactory::SDLLO) {
			return new V9990SDLRasterizer<Uint16, Renderer::ZOOM_256>(
				vdp, screen );
		} else {
			return new V9990SDLRasterizer<Uint16, Renderer::ZOOM_REAL>(
				vdp, screen );
		}
	case 4:
		if (id == RendererFactory::SDLLO) {
			return new V9990SDLRasterizer<Uint32, Renderer::ZOOM_256>(
				vdp, screen );
		} else {
			return new V9990SDLRasterizer<Uint32, Renderer::ZOOM_REAL>(
				vdp, screen );
		}
	default:
		assert(false);
		return 0;
	}
}

// TODO: If we can switch video system at any time (not just frame end),
//       is this polling approach necessary at all?
// TODO: Code is exactly the same as SDLGLVideoSystem::checkSettings.
bool SDLVideoSystem::checkSettings()
{
	// Check full screen setting.
	bool fullScreenState = (screen->flags & SDL_FULLSCREEN) != 0;
	const bool fullScreenTarget =
		RenderSettings::instance().getFullScreen()->getValue();
	if (fullScreenState == fullScreenTarget) return true;

#ifdef _WIN32
	// Under win32, toggling full screen requires opening a new SDL screen.
	return false;
#else
	// Try to toggle full screen.
	SDL_WM_ToggleFullScreen(screen);
	fullScreenState =
		(((volatile SDL_Surface*)screen)->flags & SDL_FULLSCREEN) != 0;
	return fullScreenState == fullScreenTarget;
#endif
}

bool SDLVideoSystem::prepare()
{
	if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) {
		return false;
	}
	return true;
}

void SDLVideoSystem::flush()
{
	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	SDL_Flip(screen);
}

void SDLVideoSystem::takeScreenShot(const string& filename)
{
	if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) {
		throw CommandException("Failed to lock surface.");
	}
	try {
		ScreenShotSaver::save(screen, filename);
		if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	} catch (CommandException& e) {
		if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		throw;
	}
}

} // namespace openmsx

