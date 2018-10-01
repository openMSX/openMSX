#include "SDLVisibleSurface.hh"
#include "SDLOffScreenSurface.hh"
#include "PNG.hh"
#include "SDLSnow.hh"
#include "OSDConsoleRenderer.hh"
#include "OSDGUILayer.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "MSXException.hh"
#include "unreachable.hh"
#include "vla.hh"
#include "build-info.hh"
#include <cstdint>
#include <memory>

namespace openmsx {

SDLVisibleSurface::SDLVisibleSurface(
		unsigned width, unsigned height,
		Display& display_,
		RTScheduler& rtScheduler_,
		EventDistributor& eventDistributor_,
		InputEventGenerator& inputEventGenerator_,
		CliComm& cliComm_)
	: VisibleSurface(display_, rtScheduler_, eventDistributor_,
	                 inputEventGenerator_, cliComm_)
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

	createSurface(width, height, flags);
	const SDL_Surface* surf = getSDLSurface();
	setSDLFormat(*surf->format);
	setBufferPtr(static_cast<char*>(surf->pixels), surf->pitch);
}

void SDLVisibleSurface::flushFrameBuffer()
{
	unlock();
	SDL_Surface* surf = getSDLSurface();
	SDL_Renderer* render = getSDLRenderer();
	SDL_UpdateTexture(texture.get(), nullptr, surf->pixels, surf->pitch);
	SDL_RenderClear(render);
	SDL_RenderCopy(render, texture.get(), nullptr, nullptr);
}

void SDLVisibleSurface::finish()
{
	SDL_RenderPresent(getSDLRenderer());
}

std::unique_ptr<Layer> SDLVisibleSurface::createSnowLayer()
{
	switch (getSDLFormat().BytesPerPixel) {
#if HAVE_16BPP
	case 2:
		return std::make_unique<SDLSnow<uint16_t>>(*this, getDisplay());
#endif
#if HAVE_32BPP
	case 4:
		return std::make_unique<SDLSnow<uint32_t>>(*this, getDisplay());
#endif
	default:
		UNREACHABLE; return nullptr;
	}
}

std::unique_ptr<Layer> SDLVisibleSurface::createConsoleLayer(
		Reactor& reactor, CommandConsole& console)
{
	const bool openGL = false;
	return std::make_unique<OSDConsoleRenderer>(
		reactor, console, getWidth(), getHeight(), openGL);
}

std::unique_ptr<Layer> SDLVisibleSurface::createOSDGUILayer(OSDGUI& gui)
{
	return std::make_unique<SDLOSDGUILayer>(gui);
}

std::unique_ptr<OutputSurface> SDLVisibleSurface::createOffScreenSurface()
{
	return std::make_unique<SDLOffScreenSurface>(*getSDLSurface());
}

void SDLVisibleSurface::saveScreenshot(const std::string& filename)
{
	saveScreenshotSDL(*this, filename);
}

void SDLVisibleSurface::saveScreenshotSDL(
	OutputSurface& output, const std::string& filename)
{
	unsigned width = output.getWidth();
	unsigned height = output.getHeight();
	VLA(const void*, rowPointers, height);
	MemBuffer<uint8_t> buffer(width * height * 3);
	for (unsigned i = 0; i < height; ++i) {
		rowPointers[i] = &buffer[width * 3 * i];
	}
	if (SDL_RenderReadPixels(
			output.getSDLRenderer(), nullptr,
			SDL_PIXELFORMAT_RGB24, buffer.data(), width * 3)) {
		throw MSXException("Couldn't acquire screenshot pixels: ", SDL_GetError());
	}
	PNG::save(width, height, rowPointers, filename);
}

void SDLVisibleSurface::clearScreen()
{
	unlock();
	SDL_FillRect(getSDLSurface(), nullptr, 0);
}

} // namespace openmsx
