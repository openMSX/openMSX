// $Id$

#include "SDLGLVideoSystem.hh"
#include "GLUtil.hh"
#include "ScreenShotSaver.hh"
#include <SDL.h>


namespace openmsx {

// Dimensions of screen.
static const int WIDTH = 640;
static const int HEIGHT = 480;

SDLGLVideoSystem::SDLGLVideoSystem()
{
}

SDLGLVideoSystem::~SDLGLVideoSystem()
{
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

