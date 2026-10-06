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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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

std::optional<gl::vec2> SDLVideoSystem::getMsxPixelSize()
{
	return screen->getMsxPixelSize();
}

OutputSurface* SDLVideoSystem::getOutputSurface()
{
	return screen.get();
}

void SDLVideoSystem::showCursor(Cursor cursor)
{
	if (cursor == Cursor::CROSSHAIR) {
		SDL_SetCursor(getCrosshairCursor());
	} else if (crosshairCursor) {
		SDL_SetCursor(SDL_GetDefaultCursor());
		crosshairCursor.reset();
	}
	SDL_ShowCursor((cursor == Cursor::HIDDEN) ? SDL_DISABLE : SDL_ENABLE);
	// Only let ImGui change the shape of the normal cursor.
	if (cursor == Cursor::NORMAL) {
		ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
	} else {
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	}
}

void SDLVideoSystem::updateCursor()
{
	screen->updateCursor();
}

SDL_Cursor* SDLVideoSystem::getCrosshairCursor()
{
	// The crosshair is 16 MSX pixels wide, so it scales with the window.
	int size = std::clamp(int(16.0f * screen->getMsxPixelSize().x), 24, 128) & ~1;
	if (crosshairCursor && (crosshairSize == size)) return crosshairCursor.get();

	// Red ring and cross with a black outline, so it's visible on both dark
	// and bright backgrounds, and a white hot spot. The shape is designed
	// for a 32x32 cursor and scaled by 's'.
	static constexpr float RING_RADIUS = 9.0f;
	static constexpr float ARM_START = 4.0f;
	static constexpr float ARM_END = 14.0f;
	static constexpr uint32_t CLEAR = 0x00000000; // ARGB
	static constexpr uint32_t RED         = 0xFFFF2020;
	static constexpr uint32_t BLACK       = 0xFF000000;
	static constexpr uint32_t WHITE       = 0xFFFFFFFF;
	float s = float(size) / 32.0f;
	int c = size / 2;
	int half = int(s * 0.5f); // half the line thickness
	int outline = std::max(1, int(s + 0.5f));
	auto inShape = [&](int x, int y) {
		int dx = x - c, dy = y - c;
		float d = std::sqrt(float(dx * dx + dy * dy));
		bool ring = (d >= RING_RADIUS * s) && (d <= RING_RADIUS * s + float(2 * half + 1));
		int arm = std::max(std::abs(dx), std::abs(dy));
		bool cross = ((std::abs(dx) <= half) || (std::abs(dy) <= half)) &&
		             (arm >= int(ARM_START * s)) && (arm <= int(ARM_END * s));
		return ring || cross;
	};
	auto nearShape = [&](int x, int y) {
		for (int oy = -outline; oy <= outline; ++oy) {
			for (int ox = -outline; ox <= outline; ++ox) {
				if (inShape(x + ox, y + oy)) return true;
			}
		}
		return false;
	};

	SDLSurfacePtr surf(SDL_CreateRGBSurfaceWithFormat(
		0, size, size, 32, SDL_PIXELFORMAT_ARGB8888));
	auto* pixels = static_cast<uint32_t*>(surf->pixels);
	int pitch = surf->pitch / 4;
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			bool hotSpot = (std::abs(x - c) <= half) && (std::abs(y - c) <= half);
			pixels[y * pitch + x] = hotSpot       ? WHITE
			                      : inShape(x, y)   ? RED
			                      : nearShape(x, y) ? BLACK
			                                        : CLEAR;
		}
	}
	crosshairCursor.reset(SDL_CreateColorCursor(surf.get(), c, c));
	if (!crosshairCursor) {
		// Not every platform supports color cursors.
		crosshairCursor.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR));
	}
	crosshairSize = size;
	return crosshairCursor.get();
}

bool SDLVideoSystem::getCursorEnabled()
{
	return SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE;
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
