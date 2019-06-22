#include "VisibleSurface.hh"
#include "InitException.hh"
#include "Icon.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "SDLSurfacePtr.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "InputEventGenerator.hh"
#include "PNG.hh"
#include "FileContext.hh"
#include "CliComm.hh"
#include "build-info.hh"
#include <cassert>

namespace openmsx {

VisibleSurface::VisibleSurface(
		Display& display_,
		RTScheduler& rtScheduler,
		EventDistributor& eventDistributor_,
		InputEventGenerator& inputEventGenerator_,
		CliComm& cliComm_)
	: RTSchedulable(rtScheduler)
	, display(display_)
	, eventDistributor(eventDistributor_)
	, inputEventGenerator(inputEventGenerator_)
	, cliComm(cliComm_)
{
	auto& renderSettings = display.getRenderSettings();

	inputEventGenerator_.getGrabInput().attach(*this);
	renderSettings.getPointerHideDelaySetting().attach(*this);
	renderSettings.getFullScreenSetting().attach(*this);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_MOTION_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_UP_EVENT, *this);

	updateCursor();
}

// TODO: The video subsystem is not de-inited on errors.
//       While it would be consistent to do so, doing it in this class is
//       not ideal since the init doesn't happen here.
void VisibleSurface::createSurface(int width, int height, unsigned flags)
{
	if (getDisplay().getRenderSettings().getFullScreen()) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	flags |= SDL_WINDOW_ALLOW_HIGHDPI;

	assert(!window);
	window.reset(SDL_CreateWindow(
			display.getWindowTitle().c_str(),
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			width, height,
			flags));
	if (!window) {
		std::string err = SDL_GetError();
		throw InitException("Could not create window: ", err);
	}

	updateWindowTitle();

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

	// prefer linear filtering (instead of nearest neighbour)
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	// set icon
	if (OPENMSX_SET_WINDOW_ICON) {
		SDLSurfacePtr iconSurf;
		// always use 32x32 icon on Windows, for some reason you get badly scaled icons there
#ifndef _WIN32
		try {
			iconSurf = PNG::load(preferSystemFileContext().resolve("icons/openMSX-logo-256.png"), true);
		} catch (MSXException& e) {
			cliComm.printWarning(
				"Falling back to built in 32x32 icon, because failed to load icon: ",
				e.getMessage());
#endif
			iconSurf.reset(SDL_CreateRGBSurfaceFrom(
				const_cast<char*>(openMSX_icon.pixel_data),
				openMSX_icon.width, openMSX_icon.height,
				openMSX_icon.bytes_per_pixel * 8,
				openMSX_icon.bytes_per_pixel * openMSX_icon.width,
				OPENMSX_BIGENDIAN ? 0xFF000000 : 0x000000FF,
				OPENMSX_BIGENDIAN ? 0x00FF0000 : 0x0000FF00,
				OPENMSX_BIGENDIAN ? 0x0000FF00 : 0x00FF0000,
				OPENMSX_BIGENDIAN ? 0x000000FF : 0xFF000000));
#ifndef _WIN32
		}
#endif
		SDL_SetColorKey(iconSurf.get(), SDL_TRUE, 0);
		SDL_SetWindowIcon(window.get(), iconSurf.get());
	}
}

VisibleSurface::~VisibleSurface()
{
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_MOTION_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_BUTTON_UP_EVENT, *this);
	inputEventGenerator.getGrabInput().detach(*this);
	auto& renderSettings = display.getRenderSettings();
	renderSettings.getPointerHideDelaySetting().detach(*this);
	renderSettings.getFullScreenSetting().detach(*this);
}

void VisibleSurface::updateWindowTitle()
{
	assert(window);
	SDL_SetWindowTitle(window.get(), display.getWindowTitle().c_str());
}

bool VisibleSurface::setFullScreen(bool fullscreen)
{
	auto flags = SDL_GetWindowFlags(window.get());
	// Note: SDL_WINDOW_FULLSCREEN_DESKTOP also has the SDL_WINDOW_FULLSCREEN
	//       bit set.
	bool currentState = (flags & SDL_WINDOW_FULLSCREEN) != 0;
	if (currentState == fullscreen) {
		// already wanted stated
		return true;
	}

	// in win32, toggling full screen requires opening a new SDL screen
	// in Linux calling the SDL_WM_ToggleFullScreen usually works fine
	// We now always create a new screen to make the code on both OSes
	// more similar (we had a windows-only bug because of this difference)
	return false;

	/*
	// try to toggle full screen
	SDL_Surface* surf = getSDLSurface();
	SDL_WM_ToggleFullScreen(surf);
	bool newState = (surf->flags & SDL_FULLSCREEN) != 0;
	return newState == fullscreen;
	*/
}

void VisibleSurface::update(const Setting& /*setting*/)
{
	updateCursor();
}

void VisibleSurface::executeRT()
{
	// timer expired, hide cursor
	SDL_ShowCursor(SDL_DISABLE);
}

int VisibleSurface::signalEvent(const std::shared_ptr<const Event>& event)
{
	EventType type = event->getType();
	assert((type == OPENMSX_MOUSE_MOTION_EVENT) ||
	       (type == OPENMSX_MOUSE_BUTTON_UP_EVENT) ||
	       (type == OPENMSX_MOUSE_BUTTON_DOWN_EVENT));
	(void)type;
	updateCursor();
	return 0;
}

void VisibleSurface::updateCursor()
{
	cancelRT();
	auto& renderSettings = display.getRenderSettings();
	if (renderSettings.getFullScreen() ||
	    inputEventGenerator.getGrabInput().getBoolean()) {
		// always hide cursor in fullscreen or grabinput mode
		SDL_ShowCursor(SDL_DISABLE);
		return;
	}
	float delay = renderSettings.getPointerHideDelay();
	if (delay == 0.0f) {
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_ShowCursor(SDL_ENABLE);
		if (delay > 0.0f) {
			scheduleRT(int(delay * 1e6f)); // delay in s, schedule in us
		}
	}
}

} // namespace openmsx
