// $Id$

#include "SDLVideoSystem.hh"
#include "SDLRenderer.hh"
#include "SDLSnow.hh"
#include "SDLConsole.hh"
#include "Display.hh"
#include "RendererFactory.hh"
#include "RenderSettings.hh"
#include "SDLUtil.hh"
#include "CommandConsole.hh"
#include "InitException.hh"
#include "BooleanSetting.hh"
#include "ScreenShotSaver.hh"
#include <SDL.h>
#include <cassert>

#ifdef __WIN32__
#include <windows.h>
static int lastWindowX = 0;
static int lastWindowY = 0;
#endif 


namespace openmsx {

SDLVideoSystem::SDLVideoSystem(
	VDP* vdp, RendererFactory::RendererID id )
{
	assert(id == RendererFactory::SDLHI || id == RendererFactory::SDLLO);
	const unsigned WIDTH = (id == RendererFactory::SDLLO) ? 320 : 640;
	const unsigned HEIGHT = (id == RendererFactory::SDLLO) ? 240 : 480;

	// Destruct old layers, so resources are freed before new allocations
	// are done.
	Display::INSTANCE.reset();

	bool fullScreen = RenderSettings::instance().getFullScreen()->getValue();
	int flags = SDL_SWSURFACE | (fullScreen ? SDL_FULLSCREEN : 0);

	initSDLVideo();

	// Try default bpp.
	screen = SDL_SetVideoMode(WIDTH, HEIGHT, 0, flags);
	// Can we handle this bbp?
	int bytepp = (screen ? screen->format->BytesPerPixel : 0);
	if (bytepp != 1 && bytepp != 2 && bytepp != 4) {
		screen = NULL;
	}
	// Try supported bpp in order of preference.
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 15, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 16, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 32, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 8, flags);

	if (!screen) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		throw InitException("could not open any screen");
	}
	PRT_DEBUG("Display is " << (int)(screen->format->BitsPerPixel) << " bpp.");

	// Hide mouse cursor.
	SDL_ShowCursor(SDL_DISABLE);

#ifdef __WIN32__
	// Find our current location
	HWND handle = GetActiveWindow();
	RECT windowRect;
	GetWindowRect (handle, &windowRect);
	// and adjust if needed
	if ((windowRect.right < 0) || (windowRect.bottom < 0)){
		SetWindowPos(handle, HWND_TOP,lastWindowX,lastWindowY,0,0,SWP_NOSIZE);
	}
#endif 

	Display* display = new Display(auto_ptr<VideoSystem>(this));
	Display::INSTANCE.reset(display);
	Renderer* renderer;
	Layer* background;
	switch (screen->format->BytesPerPixel) {
	case 1:
		renderer = (id == RendererFactory::SDLLO)
			? static_cast<Renderer*>(
				new SDLRenderer<Uint8, Renderer::ZOOM_256>(id, vdp, screen) )
			: static_cast<Renderer*>(
				new SDLRenderer<Uint8, Renderer::ZOOM_REAL>(id, vdp, screen) )
			;
		background = new SDLSnow<Uint8>(screen);
		break;
	case 2:
		renderer = (id == RendererFactory::SDLLO)
			? static_cast<Renderer*>(
				new SDLRenderer<Uint16, Renderer::ZOOM_256>(id, vdp, screen) )
			: static_cast<Renderer*>(
				new SDLRenderer<Uint16, Renderer::ZOOM_REAL>(id, vdp, screen) )
			;
		background = new SDLSnow<Uint16>(screen);
		break;
	case 4:
		renderer = (id == RendererFactory::SDLLO)
			? static_cast<Renderer*>(
				new SDLRenderer<Uint32, Renderer::ZOOM_256>(id, vdp, screen) )
			: static_cast<Renderer*>(
				new SDLRenderer<Uint32, Renderer::ZOOM_REAL>(id, vdp, screen) )
			;
		background = new SDLSnow<Uint32>(screen);
		break;
	default:
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		throw InitException("unsupported colour depth");
	}
	display->addLayer(background);
	display->setAlpha(background, 255);
	Layer* rendererLayer = dynamic_cast<Layer*>(renderer);
	display->addLayer(rendererLayer);
	display->setAlpha(rendererLayer, 255);
	new SDLConsole(CommandConsole::instance(), screen);

	this->renderer = renderer;
}

SDLVideoSystem::~SDLVideoSystem()
{
#ifdef __WIN32__
	// Find our current location
	if ((screen->flags & SDL_FULLSCREEN) == 0){
		HWND handle = GetActiveWindow();
		RECT windowRect;
		GetWindowRect (handle, &windowRect);
		lastWindowX = windowRect.left;
		lastWindowY = windowRect.top;
	}
#endif

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
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

#ifdef __WIN32__
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

