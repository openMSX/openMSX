// $Id$

#include "SDLGLVideoSystem.hh"
#include "PixelRenderer.hh"
#include "GLRasterizer.hh"
#include "GLSnow.hh"
#include "GLConsole.hh"
#include "GLUtil.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "SDLUtil.hh"
#include "CommandConsole.hh"
#include "InitException.hh"
#include "BooleanSetting.hh"
#include "ScreenShotSaver.hh"
#include <SDL.h>


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

	screen = openSDLVideo(WIDTH, HEIGHT,
		SDL_OPENGL | SDL_HWSURFACE | SDL_DOUBLEBUF );

	Display* display = new Display(auto_ptr<VideoSystem>(this));
	Display::INSTANCE.reset(display);
	GLSnow* background = new GLSnow();
	rasterizer = new GLRasterizer(vdp);
	display->addLayer(background);
	display->setAlpha(background, 255);
	display->addLayer(rasterizer);
	display->setAlpha(rasterizer, 255);
	new GLConsole(CommandConsole::instance());
}

SDLGLVideoSystem::~SDLGLVideoSystem()
{
	closeSDLVideo(screen);
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

