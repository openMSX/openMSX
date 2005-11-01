// $Id$

#include "OutputSurface.hh"
#include "HardwareConfig.hh"
#include "InitException.hh"
#include "Icon.hh"
#include "Version.hh"
#include "build-info.hh"

#ifdef _WIN32
#include <windows.h>
static int lastWindowX = 0;
static int lastWindowY = 0;
#endif

namespace openmsx {

OutputSurface::OutputSurface()
	: surface(0)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO) &&
	    SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		throw InitException(
		   std::string("SDL video init failed: ") + SDL_GetError());
	}

	// set window title
	HardwareConfig& hwConfig = HardwareConfig::instance();
	std::string title = Version::FULL_VERSION + " - " +
		hwConfig.getChild("info").getChildData("manufacturer") + " " +
		hwConfig.getChild("info").getChildData("code");
	SDL_WM_SetCaption(title.c_str(), 0);

	// set icon
	SDL_Surface* iconSurf = SDL_CreateRGBSurfaceFrom(
		const_cast<char*>(openMSX_icon.pixel_data),
		openMSX_icon.width, openMSX_icon.height,
		openMSX_icon.bytes_per_pixel * 8,
		openMSX_icon.bytes_per_pixel * openMSX_icon.width,
		OPENMSX_BIGENDIAN ? 0xFF000000 : 0x000000FF,
		OPENMSX_BIGENDIAN ? 0x00FF0000 : 0x0000FF00,
		OPENMSX_BIGENDIAN ? 0x0000FF00 : 0x00FF0000,
		OPENMSX_BIGENDIAN ? 0x000000FF : 0xFF000000);
	SDL_SetColorKey(iconSurf, SDL_SRCCOLORKEY, 0);
	SDL_WM_SetIcon(iconSurf, NULL);
	SDL_FreeSurface(iconSurf);

	// hide mouse cursor
	SDL_ShowCursor(SDL_DISABLE);
}

void OutputSurface::createSurface(unsigned width, unsigned height, int flags)
{
	// try default bpp
	surface = SDL_SetVideoMode(width, height, 0, flags);
	int bytepp = (surface ? surface->format->BytesPerPixel : 0);
	if (bytepp != 2 && bytepp != 4) {
		surface = NULL;
	}
	// try supported bpp in order of preference
	if (!surface) surface = SDL_SetVideoMode(width, height, 15, flags);
	if (!surface) surface = SDL_SetVideoMode(width, height, 16, flags);
	if (!surface) surface = SDL_SetVideoMode(width, height, 32, flags);

	if (!surface) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		throw InitException("Could not open any screen");
	}

#ifdef _WIN32
	// find our current location...
	HWND handle = GetActiveWindow();
	RECT windowRect;
	GetWindowRect(handle, &windowRect);
	// ...and adjust if needed
	if ((windowRect.right < 0) || (windowRect.bottom < 0)) {
		SetWindowPos(handle, HWND_TOP, lastWindowX, lastWindowY,
		             0, 0, SWP_NOSIZE);
	}
#endif
}

OutputSurface::~OutputSurface()
{
#ifdef _WIN32
	// Find our current location.
	if (surface && ((surface->flags & SDL_FULLSCREEN) == 0)) {
		HWND handle = GetActiveWindow();
		RECT windowRect;
		GetWindowRect(handle, &windowRect);
		lastWindowX = windowRect.left;
		lastWindowY = windowRect.top;
	}
#endif
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool OutputSurface::setFullScreen(bool wantedState)
{
	bool currentState = (surface->flags & SDL_FULLSCREEN) != 0;
	if (currentState == wantedState) {
		// already wanted stated
		return true;
	}

#ifdef _WIN32
	// under win32, toggling full screen requires opening a new SDL screen
	return false;
#else
	// try to toggle full screen
	SDL_WM_ToggleFullScreen(surface);
	bool newState = (surface->flags & SDL_FULLSCREEN) != 0;
	return newState == wantedState;
#endif
}

} // namespace openmsx
