// $Id$

#include "SDLGLVideoSystem.hh"
#include "SDLGLRenderer.hh"
#include "GLSnow.hh"
#include "GLConsole.hh"
#include "GLUtil.hh"
#include "Display.hh"
#include "RendererFactory.hh"
#include "RenderSettings.hh"
#include "SDLUtil.hh"
#include "CommandConsole.hh"
#include "InitException.hh"
#include "BooleanSetting.hh"
#include "ScreenShotSaver.hh"
#include <SDL.h>

#ifdef __WIN32__
#include <windows.h>
static int lastWindowX = 0;
static int lastWindowY = 0;
#endif


namespace openmsx {

// Dimensions of screen.
static const int WIDTH = 640;
static const int HEIGHT = 480;

SDLGLVideoSystem::SDLGLVideoSystem(VDP* vdp)
{
	// Destruct old layers, so resources are freed before new allocations
	// are done.
	// TODO: This has to be done before every video system (re)init,
	//       so move it to a central location.
	Display::INSTANCE.reset();

	// TODO: Probably rather similar to SDLVideoSystem;
	//       move some code to SDLUtil?
	bool fullScreen = RenderSettings::instance().getFullScreen()->getValue();
	int flags = SDL_OPENGL | SDL_HWSURFACE
		| (fullScreen ? SDL_FULLSCREEN : 0);

	initSDLVideo();

	// Enables OpenGL double buffering.
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);

	// Try default bpp.
	screen = SDL_SetVideoMode(WIDTH, HEIGHT, 0, flags);
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

	Display* display = new Display(auto_ptr<VideoSystem>(this));
	Display::INSTANCE.reset(display);
	GLSnow* background = new GLSnow();
	SDLGLRenderer* renderer = new SDLGLRenderer(RendererFactory::SDLGL, vdp);
	display->addLayer(background);
	display->setAlpha(background, 255);
	display->addLayer(renderer);
	display->setAlpha(renderer, 255);
	new GLConsole(CommandConsole::instance());

	this->renderer = renderer;
}

SDLGLVideoSystem::~SDLGLVideoSystem()
{
#ifdef __WIN32__
	// Find our current location.
	if ((screen->flags & SDL_FULLSCREEN) == 0) {
		HWND handle = GetActiveWindow();
		RECT windowRect;
		GetWindowRect(handle, &windowRect);
		lastWindowX = windowRect.left;
		lastWindowY = windowRect.top;
	}
#endif

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

// TODO: If we can switch video system at any time (not just frame end),
//       is this polling approach necessary at all?
bool SDLGLVideoSystem::checkSettings()
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

void SDLGLVideoSystem::flush()
{
	SDL_GL_SwapBuffers();
}

void SDLGLVideoSystem::takeScreenShot(const string& filename)
{
	byte* row_pointers[HEIGHT];
	byte buffer[WIDTH * HEIGHT * 3];
	for (int i = 0; i < HEIGHT; ++i) {
		row_pointers[HEIGHT - 1 - i] = &buffer[WIDTH * 3 * i];
	}
	glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	ScreenShotSaver::save(WIDTH, HEIGHT, row_pointers, filename);
}

} // namespace openmsx

