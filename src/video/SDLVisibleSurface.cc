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
	: SDLVisibleSurfaceBase(display_, rtScheduler_, eventDistributor_,
	                        inputEventGenerator_, cliComm_, videoSystem_)
{
	int flags = 0;
	createSurface(width, height, flags);

	renderer.reset(SDL_CreateRenderer(window.get(), -1, 0));
	if (!renderer) {
		std::string err = SDL_GetError();
		throw InitException("Could not create renderer: " + err);
	}
	SDL_RenderSetLogicalSize(renderer.get(), width, height);
	setSDLRenderer(renderer.get());

	surface.reset(SDL_CreateRGBSurface(
		0, width, height, 32,
		0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000));
	if (!surface) {
		std::string err = SDL_GetError();
		throw InitException("Could not create surface: " + err);
	}
	setSDLSurface(surface.get());

	texture.reset(SDL_CreateTexture(
		renderer.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		width, height));
	if (!texture) {
		std::string err = SDL_GetError();
		throw InitException("Could not create texture: " + err);
	}

	setSDLPixelFormat(*surface->format);
	setBufferPtr(static_cast<char*>(surface->pixels), surface->pitch);

	// In the SDL renderer logical size is the same as physical size.
	gl::ivec2 size(width, height);
	calculateViewPort(size, size);
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
	switch (getPixelFormat().getBytesPerPixel()) {
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
	auto [width, height] = getLogicalSize();
	return std::make_unique<OSDConsoleRenderer>(
		reactor, console, width, height, openGL);
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
	auto [width, height] = output.getLogicalSize();
	VLA(const void*, rowPointers, height);
	MemBuffer<uint8_t> buffer(width * height * 3);
	for (int i = 0; i < height; ++i) {
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
