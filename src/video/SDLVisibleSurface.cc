// $Id$

#include "SDLVisibleSurface.hh"
#include "SDLOffScreenSurface.hh"
#include "PNG.hh"
#include "SDLSnow.hh"
#include "OSDConsoleRenderer.hh"
#include "OSDGUILayer.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include <cassert>
#if PLATFORM_GP2X
#include "GP2XMMUHack.hh"
#endif

namespace openmsx {

SDLVisibleSurface::SDLVisibleSurface(
		unsigned width, unsigned height, bool fullscreen,
		RenderSettings& renderSettings,
		EventDistributor& eventDistributor,
		InputEventGenerator& inputEventGenerator)
	: VisibleSurface(renderSettings, eventDistributor, inputEventGenerator)
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
#elif PLATFORM_DINGUX
	// The OpenDingux kernel supports double buffering, while the legacy
	// kernel will hang apps that try to use double buffering.
	// The Dingoo seems to have a hardware problem that makes it hard or
	// impossible to know when vsync happens:
	//   http://www.dingux.com/2009/07/on-screen-tearing.html
	// However, double buffering increases performance and does reduce
	// the tearing somewhat, so it is worth having.
	int flags = SDL_HWSURFACE | SDL_DOUBLEBUF;
#else
	int flags = SDL_SWSURFACE; // Why did we use a SW surface again?
#endif
	if (fullscreen) flags |= SDL_FULLSCREEN;

	createSurface(width, height, flags);
	setSDLFormat(*getSDLDisplaySurface()->format);

#if PLATFORM_GP2X
	const SDL_PixelFormat& format = getSDLFormat();
	SDL_Surface* workSurface = SDL_CreateRGBSurface(SDL_HWSURFACE,
		width, height, format.BitsPerPixel, format.Rmask,
		format.Gmask, format.Bmask, format.Amask);
	assert(workSurface); // TODO
	SDL_FillRect(workSurface, NULL, 0);
	GP2XMMUHack::instance().patchPageTables();
#else
	// on non-GP2X platforms, work and displaySurfaces are the same,
	// see remark above
	SDL_Surface* workSurface = getSDLDisplaySurface();
#endif
	setSDLWorkSurface(workSurface);
	setBufferPtr(static_cast<char*>(workSurface->pixels), workSurface->pitch);
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
#if !PLATFORM_GP2X
	// The pixel pointer might be invalidated by the flip.
	// This is certainly the case when double buffering.
	SDL_Surface* workSurface = getSDLDisplaySurface();
	setBufferPtr(static_cast<char*>(workSurface->pixels), workSurface->pitch);
#endif
}

std::auto_ptr<Layer> SDLVisibleSurface::createSnowLayer(Display& display)
{
	switch (getSDLFormat().BytesPerPixel) {
#if HAVE_16BPP
	case 2:
		return std::auto_ptr<Layer>(new SDLSnow<word>(*this, display));
#endif
#if HAVE_32BPP
	case 4:
		return std::auto_ptr<Layer>(new SDLSnow<unsigned>(*this, display));
#endif
	default:
		UNREACHABLE; return std::auto_ptr<Layer>();
	}
}

std::auto_ptr<Layer> SDLVisibleSurface::createConsoleLayer(
		Reactor& reactor, CommandConsole& console)
{
	const bool openGL = false;
	return std::auto_ptr<Layer>(new OSDConsoleRenderer(
		reactor, console, getWidth(), getHeight(), openGL));
}

std::auto_ptr<Layer> SDLVisibleSurface::createOSDGUILayer(OSDGUI& gui)
{
	return std::auto_ptr<Layer>(new SDLOSDGUILayer(gui));
}

std::auto_ptr<OutputSurface> SDLVisibleSurface::createOffScreenSurface()
{
	return std::auto_ptr<OutputSurface>(
		new SDLOffScreenSurface(*getSDLWorkSurface()));
}

void SDLVisibleSurface::saveScreenshot(const std::string& filename)
{
	lock();
	PNG::save(getSDLWorkSurface(), filename);
}

} // namespace openmsx
