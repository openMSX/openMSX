// $Id$

#include "SDLUtil.hh"
#include "Icon.hh"
#include "InputEventGenerator.hh"
#include "Version.hh"
#include "HardwareConfig.hh"
#include "InitException.hh"
#include "RenderSettings.hh"
#include "openmsx.hh"
#include <SDL.h>

#ifdef _WIN32
#include <windows.h>
static int lastWindowX = 0;
static int lastWindowY = 0;
#endif


namespace openmsx {

SDL_Surface* openSDLVideo(int width, int height, int flags)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO)
	&& SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		throw InitException(
			string("SDL video init failed: ") + SDL_GetError() );
	}

	// Set window title.
	HardwareConfig& hardwareConfig = HardwareConfig::instance();
	string title = Version::WINDOW_TITLE + " - " +
		hardwareConfig.getChild("info").getChildData("manufacturer") + " " +
		hardwareConfig.getChild("info").getChildData("code");
	SDL_WM_SetCaption(title.c_str(), 0);

	// Set icon.
	unsigned int iconRGBA[OPENMSX_ICON_SIZE * OPENMSX_ICON_SIZE];
	for (int i = 0; i < OPENMSX_ICON_SIZE * OPENMSX_ICON_SIZE; i++) {
		iconRGBA[i] = iconColours[iconData[i]];
	}
	SDL_Surface* iconSurf = SDL_CreateRGBSurfaceFrom(
		iconRGBA, OPENMSX_ICON_SIZE, OPENMSX_ICON_SIZE, 32,
		OPENMSX_ICON_SIZE * sizeof(unsigned int),
		0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
		);
	SDL_SetColorKey(iconSurf, SDL_SRCCOLORKEY, 0);
	SDL_WM_SetIcon(iconSurf, NULL);
	SDL_FreeSurface(iconSurf);

	// Hide mouse cursor.
	SDL_ShowCursor(SDL_DISABLE);

	// Add full screen flag if desired.
	if (RenderSettings::instance().getFullScreen()->getValue()) {
		flags |= SDL_FULLSCREEN;
	}
	// OpenGL double buffering uses GL attribute instead of flag.
	if (flags & SDL_OPENGL) {
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, (flags & SDL_DOUBLEBUF) != 0);
		flags &= ~SDL_DOUBLEBUF;
	}

	// Try default bpp.
	SDL_Surface* screen = SDL_SetVideoMode(width, height, 0, flags);
	// Can we handle this bbp?
	int bytepp = (screen ? screen->format->BytesPerPixel : 0);
	if (bytepp != 2 && bytepp != 4) {
		screen = NULL;
	}
	// Try supported bpp in order of preference.
	if (!screen) screen = SDL_SetVideoMode(width, height, 15, flags);
	if (!screen) screen = SDL_SetVideoMode(width, height, 16, flags);
	if (!screen) screen = SDL_SetVideoMode(width, height, 32, flags);

	if (!screen) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		throw InitException("could not open any screen");
	}
	PRT_DEBUG("Display is " << (int)(screen->format->BitsPerPixel) << " bpp.");

#ifdef _WIN32
	// Find our current location...
	HWND handle = GetActiveWindow();
	RECT windowRect;
	GetWindowRect(handle, &windowRect);
	// ...and adjust if needed.
	if ((windowRect.right < 0) || (windowRect.bottom < 0)) {
		SetWindowPos(
			handle, HWND_TOP, lastWindowX, lastWindowY, 0, 0, SWP_NOSIZE );
	}
#endif

	InputEventGenerator::instance().reinit();

	return screen;
}

void closeSDLVideo(SDL_Surface* screen)
{
#ifdef _WIN32
	// Find our current location.
	if ((screen->flags & SDL_FULLSCREEN) == 0) {
		HWND handle = GetActiveWindow();
		RECT windowRect;
		GetWindowRect(handle, &windowRect);
		lastWindowX = windowRect.left;
		lastWindowY = windowRect.top;
	}
#endif

	screen = screen; // avoid warning
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

} // namespace openmsx

