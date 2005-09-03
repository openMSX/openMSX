// $Id$

#include "SDLVideoSystem.hh"
#include "SDLRasterizer.hh"
#include "V9990SDLRasterizer.hh"
#include "SDLSnow.hh"
#include "SDLConsole.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "SDLUtil.hh"
#include "InitException.hh"
#include "ScreenShotSaver.hh"
#include "IconLayer.hh"
#include "MSXMotherBoard.hh"
#include <SDL.h>
#include <cassert>

using std::string;

namespace openmsx {

SDLVideoSystem::SDLVideoSystem(MSXMotherBoard& motherboard)
	: renderSettings(motherboard.getRenderSettings())
	, display(motherboard.getDisplay())
{
	// Destruct old layers, so resources are freed before new allocations
	// are done.
	display.resetVideoSystem();

	unsigned width, height;
	getWindowSize(width, height);
	screen = openSDLVideo(motherboard.getInputEventGenerator(),
	                      renderSettings, width, height, SDL_SWSURFACE);

	switch (screen->format->BytesPerPixel) {
	case 2:
		new SDLSnow<Uint16>(display, screen);
		break;
	case 4:
		new SDLSnow<Uint32>(display, screen);
		break;
	default:
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		throw InitException("unsupported colour depth");
	}
	new SDLConsole(motherboard, screen);

	Layer* iconLayer = new SDLIconLayer(motherboard.getCommandController(),
	                                    motherboard.getEventDistributor(),
	                                    display, screen);
	display.addLayer(iconLayer);

	display.setVideoSystem(this);
}

SDLVideoSystem::~SDLVideoSystem()
{
	closeSDLVideo(screen);
}

Rasterizer* SDLVideoSystem::createRasterizer(VDP& vdp)
{
	switch (screen->format->BytesPerPixel) {
	case 2:
		return new SDLRasterizer<Uint16>(vdp, screen);
	case 4:
		return new SDLRasterizer<Uint32>(vdp, screen);
	default:
		assert(false);
		return 0;
	}
}

V9990Rasterizer* SDLVideoSystem::createV9990Rasterizer(V9990& vdp)
{
	switch (screen->format->BytesPerPixel) {
	case 2:
		return new V9990SDLRasterizer<Uint16>(vdp, screen);
	case 4:
		return new V9990SDLRasterizer<Uint32>(vdp, screen);
	default:
		assert(false);
		return 0;
	}
}

void SDLVideoSystem::getWindowSize(unsigned& width, unsigned& height)
{
	if (renderSettings.getVideoSource().getValue() == VIDEO_MSX &&
	    renderSettings.getScaler().getValue() == SCALER_LOW) {
		width = 320;
		height = 240;
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
	if ((width  != (unsigned)screen->w) ||
	    (height != (unsigned)screen->h)) {
		return false;
	}
	
	// Check full screen setting.
	bool fullScreenState = (screen->flags & SDL_FULLSCREEN) != 0;
	const bool fullScreenTarget =
		renderSettings.getFullScreen().getValue();
	if (fullScreenState == fullScreenTarget) return true;

#ifdef _WIN32
	// Under win32, toggling full screen requires opening a new SDL screen.
	return false;
#else
	// Try to toggle full screen.
	SDL_WM_ToggleFullScreen(screen);
	fullScreenState =
		(((volatile SDL_Surface*)screen)->flags & SDL_FULLSCREEN) != 0;
	if (fullScreenState != fullScreenTarget) {
		return false;
	}
	return true;
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

