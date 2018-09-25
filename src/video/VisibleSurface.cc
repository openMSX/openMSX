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
#include <memory>

#ifdef _WIN32
#include <windows.h>
static int lastWindowX = 0;
static int lastWindowY = 0;
#endif

namespace openmsx {

VisibleSurface::VisibleSurface(
		Display& display_,
		RTScheduler& rtScheduler,
		EventDistributor& eventDistributor_,
		InputEventGenerator& inputEventGenerator_,
		CliComm& cliComm)
	: RTSchedulable(rtScheduler)
	, display(display_)
	, eventDistributor(eventDistributor_)
	, inputEventGenerator(inputEventGenerator_)
{
	(void)cliComm; // avoid unused parameter warning on _WIN32

	if (!SDL_WasInit(SDL_INIT_VIDEO) &&
	    SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		throw InitException("SDL video init failed: ", SDL_GetError());
	}

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
		SDL_SetColorKey(iconSurf.get(), SDL_SRCCOLORKEY, 0);
		SDL_WM_SetIcon(iconSurf.get(), nullptr);
	}

	// on Mac it seems to be necessary to grab input in full screen
	// Note: this is duplicated from InputEventGenerator::setGrabInput
	// in order to keep the settings in sync (grab when fullscreen)
	auto& renderSettings = display.getRenderSettings();
	SDL_WM_GrabInput((inputEventGenerator_.getGrabInput().getBoolean() ||
			  renderSettings.getFullScreen())
			?  SDL_GRAB_ON : SDL_GRAB_OFF);

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

void VisibleSurface::createSurface(unsigned width, unsigned height, int flags)
{
	if (getDisplay().getRenderSettings().getFullScreen()) flags |= SDL_FULLSCREEN;

	// try default bpp
	SDL_Surface* surf = SDL_SetVideoMode(width, height, 0, flags);
	int bytepp = (surf ? surf->format->BytesPerPixel : 0);
	if (bytepp != 2 && bytepp != 4) {
		surf = nullptr;
	}
#if !HAVE_16BPP
	if (bytepp == 2) {
		surf = nullptr;
	}
#endif
#if !HAVE_32BPP
	if (bytepp == 4) {
		surf = nullptr;
	}
#endif
	// try supported bpp in order of preference
#if HAVE_16BPP
	if (!surf) surf = SDL_SetVideoMode(width, height, 16, flags);
#if !PLATFORM_DINGUX
	// We have a PixelOpBase specialization for Dingux which hardcodes the
	// RGB565 pixel layout, so don't use 15 bpp there.
	if (!surf) surf = SDL_SetVideoMode(width, height, 15, flags);
#endif
#endif
#if HAVE_32BPP
	if (!surf) surf = SDL_SetVideoMode(width, height, 32, flags);
#endif

	if (!surf) {
		std::string err = SDL_GetError();
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		throw InitException("Could not open any screen: ", err);
	}
	getDisplay().setOutputScreenResolution(gl::ivec2(width, height));
	setSDLSurface(surf);

	updateWindowTitle();

#ifdef _WIN32
	// find our current location...
	HWND handle = GetActiveWindow();
	RECT windowRect;
	GetWindowRect(handle, &windowRect);
	// ...and adjust if needed
	// HWND_TOP is #defined as ((HWND)0)
	auto OPENMSX_HWND_TOP = static_cast<HWND>(nullptr);
	if ((windowRect.right < 0) || (windowRect.bottom < 0)) {
		SetWindowPos(handle, OPENMSX_HWND_TOP, lastWindowX, lastWindowY,
		             0, 0, SWP_NOSIZE);
	}
#endif
}

VisibleSurface::~VisibleSurface()
{
	getDisplay().setOutputScreenResolution({-1, -1});

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

#ifdef _WIN32
	// Find our current location.
	SDL_Surface* surf = getSDLSurface();
	if (surf && ((surf->flags & SDL_FULLSCREEN) == 0)) {
		HWND handle = GetActiveWindow();
		RECT windowRect;
		GetWindowRect(handle, &windowRect);
		lastWindowX = windowRect.left;
		lastWindowY = windowRect.top;
	}
#endif
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void VisibleSurface::updateWindowTitle()
{
	SDL_WM_SetCaption(display.getWindowTitle().c_str(), nullptr);
}

bool VisibleSurface::setFullScreen(bool fullscreen)
{
	SDL_Surface* surf = getSDLSurface();
	bool currentState = (surf->flags & SDL_FULLSCREEN) != 0;
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
