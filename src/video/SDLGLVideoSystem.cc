// $Id$

#include "SDLGLVideoSystem.hh"
#include "GLRasterizer.hh"
#include "V9990GLRasterizer.hh"
#include "GLSnow.hh"
#include "GLConsole.hh"
#include "GLUtil.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "SDLUtil.hh"
#include "ScreenShotSaver.hh"
#include "IconLayer.hh"
#include "EventDistributor.hh"
#include "InputEvents.hh"
#include <SDL.h>
#include <cstdlib>

using std::string;

namespace openmsx {

SDLGLVideoSystem::SDLGLVideoSystem(
		UserInputEventDistributor& userInputEventDistributor,
		RenderSettings& renderSettings_,
		Console& console, Display& display_)
	: renderSettings(renderSettings_)
	, display(display_)
{
	// Destruct old layers, so resources are freed before new allocations
	// are done.
	// TODO: This has to be done before every video system (re)init,
	//       so move it to a central location.
	display.resetVideoSystem();

	resize(640, 480);

	new GLSnow(display);
	new GLConsole(userInputEventDistributor, console, display);

	Layer* iconLayer = new GLIconLayer(display, screen);
	display.addLayer(iconLayer);

	display.setVideoSystem(this);

	EventDistributor::instance().registerEventListener(
		OPENMSX_RESIZE_EVENT, *this, EventDistributor::DETACHED);
}

SDLGLVideoSystem::~SDLGLVideoSystem()
{
	EventDistributor::instance().unregisterEventListener(
		OPENMSX_RESIZE_EVENT, *this, EventDistributor::DETACHED);

	closeSDLVideo(screen);
}

Rasterizer* SDLGLVideoSystem::createRasterizer(VDP& vdp)
{
	return new GLRasterizer(renderSettings, display, vdp);
}

V9990Rasterizer* SDLGLVideoSystem::createV9990Rasterizer(V9990& vdp)
{
	return new V9990GLRasterizer(vdp);
}

// TODO: If we can switch video system at any time (not just frame end),
//       is this polling approach necessary at all?
bool SDLGLVideoSystem::checkSettings()
{
	// Check full screen setting.
	bool fullScreenState = (screen->flags & SDL_FULLSCREEN) != 0;
	const bool fullScreenTarget = renderSettings.getFullScreen().getValue();
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

void SDLGLVideoSystem::flush()
{
	SDL_GL_SwapBuffers();
}

void SDLGLVideoSystem::takeScreenShot(const string& filename)
{
	unsigned width  = screen->w;
	unsigned height = screen->h;
	byte** row_pointers = static_cast<byte**>(
		malloc(height * sizeof(byte*)));
	byte* buffer = static_cast<byte*>(
		malloc(width * height * 3 * sizeof(byte)));
	for (unsigned i = 0; i < height; ++i) {
		row_pointers[height - 1 - i] = &buffer[width * 3 * i];
	}
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);
	try {
		ScreenShotSaver::save(width, height, row_pointers, filename);
	} catch(...) {
		free(row_pointers);
		free(buffer);
		throw;
	}
	free(row_pointers);
	free(buffer);
}

void SDLGLVideoSystem::resize(unsigned x, unsigned y)
{
	int flags = SDL_OPENGL | SDL_HWSURFACE | SDL_DOUBLEBUF;
	//flags |= SDL_RESIZABLE;
	screen = openSDLVideo(renderSettings, x, y, flags);

	glViewport(0, 0, x, y);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 640, 480, 0, -1, 1); // coordinate system always 640x480
	glMatrixMode(GL_MODELVIEW);
}

void SDLGLVideoSystem::signalEvent(const Event& event)
{
	assert(event.getType() == OPENMSX_RESIZE_EVENT);
	const ResizeEvent& resizeEvent = static_cast<const ResizeEvent&>(event);
	resize(resizeEvent.getX(), resizeEvent.getY());
}

} // namespace openmsx

