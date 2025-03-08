#include "SDLVideoSystem.hh"

#include "Display.hh"
#include "GlobalSettings.hh"
#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "SDLRasterizer.hh"
#include "V9990.hh"
#include "V9990SDLRasterizer.hh"
#include "VDP.hh"
#include "VisibleSurface.hh"

#include "EventDistributor.hh"
#include "IntegerSetting.hh"
#include "Reactor.hh"

#include "unreachable.hh"

#include "imgui.h"

#include <memory>

#include "components.hh"
#if COMPONENT_LASERDISC
#include "LDSDLRasterizer.hh"
#include "LaserdiscPlayer.hh"
#endif

namespace openmsx {

SDLVideoSystem::SDLVideoSystem(Reactor& reactor_)
	: reactor(reactor_)
	, display(reactor.getDisplay())
	, renderSettings(reactor.getDisplay().getRenderSettings())
{
	screen = std::make_unique<VisibleSurface>(
		display,
		reactor.getRTScheduler(), reactor.getEventDistributor(),
		reactor.getInputEventGenerator(), reactor.getCliComm(),
		*this, reactor.getGlobalSettings().getPauseSetting());

	snowLayer = screen->createSnowLayer();
	osdGuiLayer = screen->createOSDGUILayer(display.getOSDGUI());
	imGuiLayer = screen->createImGUILayer(reactor.getImGuiManager());
	display.addLayer(*snowLayer);
	display.addLayer(*osdGuiLayer);
	display.addLayer(*imGuiLayer);

	renderSettings.getFullScreenSetting().attach(*this);
	renderSettings.getScaleFactorSetting().attach(*this);
}

SDLVideoSystem::~SDLVideoSystem()
{
	renderSettings.getScaleFactorSetting().detach(*this);
	renderSettings.getFullScreenSetting().detach(*this);

	display.removeLayer(*imGuiLayer);
	display.removeLayer(*osdGuiLayer);
	display.removeLayer(*snowLayer);
}

std::unique_ptr<Rasterizer> SDLVideoSystem::createRasterizer(VDP& vdp)
{
	assert(display.getRenderer() == RenderSettings::RendererID::SDLGL_PP);
	std::string videoSource = (vdp.getName() == "VDP")
	                        ? "MSX" // for backwards compatibility
	                        : vdp.getName();
	auto& motherBoard = vdp.getMotherBoard();
	return std::make_unique<SDLRasterizer>(
		vdp, display, *screen,
		std::make_unique<PostProcessor>(
			motherBoard, display, *screen,
			videoSource, 640, 240, true));
}

std::unique_ptr<V9990Rasterizer> SDLVideoSystem::createV9990Rasterizer(
	V9990& vdp)
{
	assert(display.getRenderer() == RenderSettings::RendererID::SDLGL_PP);
	std::string videoSource = (vdp.getName() == "Sunrise GFX9000")
	                        ? "GFX9000" // for backwards compatibility
	                        : vdp.getName();
	MSXMotherBoard& motherBoard = vdp.getMotherBoard();
	return std::make_unique<V9990SDLRasterizer>(
		vdp, display, *screen,
		std::make_unique<PostProcessor>(
			motherBoard, display, *screen,
			videoSource, 1280, 240, true));
}

#if COMPONENT_LASERDISC
std::unique_ptr<LDRasterizer> SDLVideoSystem::createLDRasterizer(
	LaserdiscPlayer& ld)
{
	assert(display.getRenderer() == RenderSettings::RendererID::SDLGL_PP);
	std::string videoSource = "Laserdisc"; // TODO handle multiple???
	MSXMotherBoard& motherBoard = ld.getMotherBoard();
	return std::make_unique<LDSDLRasterizer>(
		std::make_unique<PostProcessor>(
			motherBoard, display, *screen,
			videoSource, 640, 480, false));
}
#endif

void SDLVideoSystem::flush()
{
	screen->finish();
}

void SDLVideoSystem::takeScreenShot(const std::string& filename, bool withOsd)
{
	if (withOsd) {
		// we can directly save current content as screenshot
		screen->saveScreenshot(filename);
	} else {
		// we first need to re-render to an off-screen surface
		// with OSD layers disabled
		ScopedLayerHider hideOsd(*osdGuiLayer);
		ScopedLayerHider hideImgui(*imGuiLayer);
		std::unique_ptr<OutputSurface> surf = screen->createOffScreenSurface();
		display.repaintImpl(*surf);
		surf->saveScreenshot(filename);
	}
}

void SDLVideoSystem::updateWindowTitle()
{
	screen->updateWindowTitle();
}

std::optional<gl::ivec2> SDLVideoSystem::getMouseCoord()
{
	return screen->getMouseCoord();
}

OutputSurface* SDLVideoSystem::getOutputSurface()
{
	return screen.get();
}

void SDLVideoSystem::showCursor(bool show)
{
	// TODO: Is it worth checking for errors?
	if (show) {
		SDL_ShowCursor();
		ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
	} else {
		SDL_HideCursor();
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	}
}

bool SDLVideoSystem::getCursorEnabled()
{
	return SDL_CursorVisible();
}

std::string SDLVideoSystem::getClipboardText()
{
	std::string result;
	if (char* text = SDL_GetClipboardText()) {
		result = text;
		SDL_free(text);
	}
	return result;
}

void SDLVideoSystem::setClipboardText(zstring_view text)
{
	if (SDL_SetClipboardText(text.c_str()) != 0) {
		const char* err = SDL_GetError();
		SDL_ClearError();
		throw CommandException(err);
	}
}

std::optional<gl::ivec2> SDLVideoSystem::getWindowPosition()
{
	return screen->getWindowPosition();
}

void SDLVideoSystem::setWindowPosition(gl::ivec2 pos)
{
	screen->setWindowPosition(pos);
}

void SDLVideoSystem::repaint()
{
	// With SDL we can simply repaint the display directly.
	display.repaintImpl();
}

void SDLVideoSystem::update(const Setting& subject) noexcept
{
	if (&subject == &renderSettings.getFullScreenSetting()) {
		screen->setFullScreen(renderSettings.getFullScreen());
	} else if (&subject == &renderSettings.getScaleFactorSetting()) {
		screen->resize();
	} else {
		UNREACHABLE;
	}
}

bool SDLVideoSystem::signalEvent(const Event& /*event*/)
{
	// TODO: Currently window size depends only on scale factor.
	//       Maybe in the future it will be handled differently.
	//const auto& resizeEvent = get_event<ResizeEvent>(event);
	//resize(resizeEvent.getX(), resizeEvent.getY());
	//resize();
	return false;
}

} // namespace openmsx
