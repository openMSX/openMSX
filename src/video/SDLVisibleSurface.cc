// $Id$

#include "SDLVisibleSurface.hh"
#include "ScreenShotSaver.hh"
#include "CommandException.hh"
#include "SDLSnow.hh"
#include "OSDConsoleRenderer.hh"
#include "OSDGUILayer.hh"
#include "build-info.hh"
#include <cstring>
#include <cassert>
#if PLATFORM_GP2X
#include "GP2XMMUHack.hh"
#endif

namespace openmsx {

SDLVisibleSurface::SDLVisibleSurface(
		unsigned width, unsigned height, bool fullscreen)
{
#if PLATFORM_GP2X
	// We don't use HW double buffering, because that implies a vsync and
	// that cause a too big performance drop (with vsync, if you're
	// slightly too slow to run at 60fps, framerate immediately drops to
	// 30fps). Instead we simulate double buffering by rendering to an
	// extra work surface and when rendering a frame is finished, we copy
	// the work surface to the display surface (the HW blitter on GP2X is
	// fast enough).
	int flags = SDL_HWSURFACE; // | SDL_DOUBLEBUF;
#else
	int flags = SDL_SWSURFACE; // Why did we use a SW surface again?
#endif
	if (fullscreen) flags |= SDL_FULLSCREEN;

	createSurface(width, height, flags);

	SDL_PixelFormat& format = getSDLFormat();
	SDL_Surface* displaySurface = getSDLDisplaySurface();
	memcpy(&format, displaySurface->format, sizeof(SDL_PixelFormat));

#if PLATFORM_GP2X
	SDL_Surface* workSurface = SDL_CreateRGBSurface(SDL_HWSURFACE,
		width, height, format.BitsPerPixel, format.Rmask,
		format.Gmask, format.Bmask, format.Amask);
	assert(workSurface); // TODO
	SDL_FillRect(workSurface, NULL, 0);
	GP2XMMUHack::instance().patchPageTables();
#else
	// on non-GP2X platforms, work and displaySurfaces are the same,
	// see remark above
	SDL_Surface* workSurface = displaySurface;
#endif
	setSDLWorkSurface(workSurface);
	setBufferPtr(static_cast<char*>(workSurface->pixels), workSurface->pitch);
}

void SDLVisibleSurface::init()
{
	// nothing
}

void SDLVisibleSurface::drawFrameBuffer()
{
	// nothing
}

void SDLVisibleSurface::finish()
{
	unlock();
#if PLATFORM_GP2X
	void* start = getSDLWorkSurface()->pixels;
	void* end   = static_cast<char*>(start) + 320 * 240 * 2;
	GP2XMMUHack::instance().flushCache(start, end, 0);
	SDL_BlitSurface(getSDLWorkSurface(),    NULL,
	                getSDLDisplaySurface(), NULL);
#endif
	SDL_Flip(getSDLDisplaySurface());
}

void SDLVisibleSurface::takeScreenShot(const std::string& filename)
{
	lock();
	try {
		ScreenShotSaver::save(getSDLWorkSurface(), filename);
	} catch (CommandException& e) {
		throw;
	}
}

std::auto_ptr<Layer> SDLVisibleSurface::createSnowLayer()
{
	switch (getSDLFormat().BytesPerPixel) {
#if HAVE_16BPP
	case 2:
		return std::auto_ptr<Layer>(new SDLSnow<word>(*this));
#endif
#if HAVE_32BPP
	case 4:
		return std::auto_ptr<Layer>(new SDLSnow<unsigned>(*this));
#endif
	default:
		assert(false);
		return std::auto_ptr<Layer>(); // avoid warning
	}
}

std::auto_ptr<Layer> SDLVisibleSurface::createConsoleLayer(
		Reactor& reactor)
{
	const bool openGL = false;
	return std::auto_ptr<Layer>(new OSDConsoleRenderer(reactor, *this, openGL));
}

std::auto_ptr<Layer> SDLVisibleSurface::createOSDGUILayer(OSDGUI& gui)
{
	return std::auto_ptr<Layer>(new SDLOSDGUILayer(gui));
}

} // namespace openmsx
