#include "SDLVisibleSurfaceBase.hh"
#include "InitException.hh"
#include "Icon.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "PNG.hh"
#include "FileContext.hh"
#include "CliComm.hh"
#include "build-info.hh"
#include "endian.hh"
#include <cassert>

namespace openmsx {

int SDLVisibleSurfaceBase::windowPosX = SDL_WINDOWPOS_UNDEFINED;
int SDLVisibleSurfaceBase::windowPosY = SDL_WINDOWPOS_UNDEFINED;

SDLVisibleSurfaceBase::~SDLVisibleSurfaceBase()
{
	// store last known position for when we recreate it
	// the window gets recreated when changing renderers, for instance.
	// Do not store if we're fullscreen, the location is the top-left
	if ((SDL_GetWindowFlags(window.get()) & SDL_WINDOW_FULLSCREEN) == 0) {
		SDL_GetWindowPosition(window.get(), &windowPosX, &windowPosY);
	}
}

// TODO: The video subsystem is not de-initialized on errors.
//       While it would be consistent to do so, doing it in this class is
//       not ideal since the init doesn't happen here.
void SDLVisibleSurfaceBase::createSurface(int width, int height, unsigned flags)
{
	if (getDisplay().getRenderSettings().getFullScreen()) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
#ifdef __APPLE__
	// See SDLGLVisibleSurface::setViewPort() for why only macos (for now).
	flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif

	assert(!window);
	window.reset(SDL_CreateWindow(
			getDisplay().getWindowTitle().c_str(),
			windowPosX, windowPosY,
			width, height,
			flags));
	if (!window) {
		std::string err = SDL_GetError();
		throw InitException("Could not create window: ", err);
	}

	updateWindowTitle();

	// prefer linear filtering (instead of nearest neighbour)
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	// set icon
	if constexpr (OPENMSX_SET_WINDOW_ICON) {
		SDLSurfacePtr iconSurf;
		// always use 32x32 icon on Windows, for some reason you get badly scaled icons there
#ifndef _WIN32
		try {
			iconSurf = PNG::load(preferSystemFileContext().resolve("icons/openMSX-logo-256.png"), true);
		} catch (MSXException& e) {
			getCliComm().printWarning(
				"Falling back to built in 32x32 icon, because failed to load icon: ",
				e.getMessage());
#endif
			iconSurf.reset(SDL_CreateRGBSurfaceFrom(
				const_cast<char*>(openMSX_icon.pixel_data),
				openMSX_icon.width, openMSX_icon.height,
				openMSX_icon.bytes_per_pixel * 8,
				openMSX_icon.bytes_per_pixel * openMSX_icon.width,
				Endian::BIG ? 0xFF000000 : 0x000000FF,
				Endian::BIG ? 0x00FF0000 : 0x0000FF00,
				Endian::BIG ? 0x0000FF00 : 0x00FF0000,
				Endian::BIG ? 0x000000FF : 0xFF000000));
#ifndef _WIN32
		}
#endif
		SDL_SetColorKey(iconSurf.get(), SDL_TRUE, 0);
		SDL_SetWindowIcon(window.get(), iconSurf.get());
	}
}

void SDLVisibleSurfaceBase::updateWindowTitle()
{
	assert(window);
	SDL_SetWindowTitle(window.get(), getDisplay().getWindowTitle().c_str());
}

bool SDLVisibleSurfaceBase::setFullScreen(bool fullscreen)
{
	auto flags = SDL_GetWindowFlags(window.get());
	// Note: SDL_WINDOW_FULLSCREEN_DESKTOP also has the SDL_WINDOW_FULLSCREEN
	//       bit set.
	bool currentState = (flags & SDL_WINDOW_FULLSCREEN) != 0;
	if (currentState == fullscreen) {
		// already wanted stated
		return true;
	}

	if (SDL_SetWindowFullscreen(window.get(),
			fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
		return false; // error, try re-creating the window
	}
	fullScreenUpdated(fullscreen);
	return true; // success
}

} // namespace openmsx
