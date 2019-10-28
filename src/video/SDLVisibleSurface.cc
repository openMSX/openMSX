#include "SDLVisibleSurface.hh"
#include "SDLOffScreenSurface.hh"
#include "PNG.hh"
#include "SDLSnow.hh"
#include "OSDConsoleRenderer.hh"
#include "OSDGUILayer.hh"
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
		CliComm& cliComm_,
		VideoSystem& videoSystem_)
	: SDLCommonVisibleSurface(display_, rtScheduler_, eventDistributor_,
	                 inputEventGenerator_, cliComm_, videoSystem_)
{
	int flags = 0;
	createSurface(width, height, flags);
	const SDL_Surface* surf = getSDLSurface();
	setSDLFormat(*surf->format);
	setBufferPtr(static_cast<char*>(surf->pixels), surf->pitch);

	// In the SDL renderer logical size is the same as physical size.
	calculateViewPort(getLogicalSize());
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
	const SDLOutputSurface& output, const std::string& filename)
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
