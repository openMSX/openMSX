#include "SDLVisibleSurface.hh"
#include "SDLOffScreenSurface.hh"
#include "PNG.hh"
#include "SDLSnow.hh"
#include "OSDConsoleRenderer.hh"
#include "OSDGUILayer.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "memory.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include <cstdint>

namespace openmsx {

SDLVisibleSurface::SDLVisibleSurface(
		unsigned width, unsigned height,
		RenderSettings& renderSettings,
		EventDistributor& eventDistributor,
		InputEventGenerator& inputEventGenerator,
		CliComm& cliComm)
	: VisibleSurface(renderSettings, eventDistributor, inputEventGenerator,
			cliComm)
{
#if PLATFORM_DINGUX
	// The OpenDingux kernel supports double buffering, while the legacy
	// kernel will hang apps that try to use double buffering.
	// The Dingoo seems to have a hardware problem that makes it hard or
	// impossible to know when vsync happens:
	//   http://www.dingux.com/2009/07/on-screen-tearing.html
	// However, double buffering increases performance and does reduce
	// the tearing somewhat, so it is worth having.
	int flags = SDL_HWSURFACE | SDL_DOUBLEBUF;
#elif PLATFORM_ANDROID
	// On Android, SDL_HWSURFACE currently crashes although the SDL Android port is
	// supposed to support it. Probably an incompatibility between how openMSX uses
	// SDL and how the SDL Android expects an app to use SDL
	int flags = SDL_SWSURFACE;
#else
	int flags = SDL_SWSURFACE; // Why did we use a SW surface again?
#endif
	if (renderSettings.getFullScreen().getBoolean()) flags |= SDL_FULLSCREEN;

	createSurface(width, height, flags);
	SDL_Surface* surface = getSDLSurface();
	setSDLFormat(*surface->format);
	setBufferPtr(static_cast<char*>(surface->pixels), surface->pitch);
}

void SDLVisibleSurface::finish()
{
	unlock();
	SDL_Surface* surface = getSDLSurface();
	SDL_Flip(surface);
	// The pixel pointer might be invalidated by the flip.
	// This is certainly the case when double buffering.
	setBufferPtr(static_cast<char*>(surface->pixels), surface->pitch);
}

std::unique_ptr<Layer> SDLVisibleSurface::createSnowLayer(Display& display)
{
	switch (getSDLFormat().BytesPerPixel) {
#if HAVE_16BPP
	case 2:
		return make_unique<SDLSnow<uint16_t>>(*this, display);
#endif
#if HAVE_32BPP
	case 4:
		return make_unique<SDLSnow<uint32_t>>(*this, display);
#endif
	default:
		UNREACHABLE; return nullptr;
	}
}

std::unique_ptr<Layer> SDLVisibleSurface::createConsoleLayer(
		Reactor& reactor, CommandConsole& console)
{
	const bool openGL = false;
	return make_unique<OSDConsoleRenderer>(
		reactor, console, getWidth(), getHeight(), openGL);
}

std::unique_ptr<Layer> SDLVisibleSurface::createOSDGUILayer(OSDGUI& gui)
{
	return make_unique<SDLOSDGUILayer>(gui);
}

std::unique_ptr<OutputSurface> SDLVisibleSurface::createOffScreenSurface()
{
	return make_unique<SDLOffScreenSurface>(*getSDLSurface());
}

void SDLVisibleSurface::saveScreenshot(const std::string& filename)
{
	lock();
	PNG::save(getSDLSurface(), filename);
}

void SDLVisibleSurface::clearScreen()
{
	unlock();
	SDL_FillRect(getSDLSurface(), nullptr, 0);
}

} // namespace openmsx
