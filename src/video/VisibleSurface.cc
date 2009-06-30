// $Id$

#include "VisibleSurface.hh"
#include "InitException.hh"
#include "Icon.hh"
#include "RenderSettings.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "InputEventGenerator.hh"
#include "build-info.hh"
#include "openmsx.hh"

#ifdef _WIN32
#include <windows.h>
static int lastWindowX = 0;
static int lastWindowY = 0;
#endif

namespace openmsx {

VisibleSurface::VisibleSurface(RenderSettings& renderSettings_,
		EventDistributor& eventDistributor_,
		InputEventGenerator& inputEventGenerator_)
	: renderSettings(renderSettings_)
	, eventDistributor(eventDistributor_)
	, inputEventGenerator(inputEventGenerator_)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO) &&
	    SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		throw InitException(
		   std::string("SDL video init failed: ") + SDL_GetError());
	}

	// set icon
	if (OPENMSX_SET_WINDOW_ICON) {
		SDL_Surface* iconSurf = SDL_CreateRGBSurfaceFrom(
			const_cast<char*>(openMSX_icon.pixel_data),
			openMSX_icon.width, openMSX_icon.height,
			openMSX_icon.bytes_per_pixel * 8,
			openMSX_icon.bytes_per_pixel * openMSX_icon.width,
			OPENMSX_BIGENDIAN ? 0xFF000000 : 0x000000FF,
			OPENMSX_BIGENDIAN ? 0x00FF0000 : 0x0000FF00,
			OPENMSX_BIGENDIAN ? 0x0000FF00 : 0x00FF0000,
			OPENMSX_BIGENDIAN ? 0x000000FF : 0xFF000000
			);
		SDL_SetColorKey(iconSurf, SDL_SRCCOLORKEY, 0);
		SDL_WM_SetIcon(iconSurf, NULL);
		SDL_FreeSurface(iconSurf);
	}

	inputEventGenerator_.getGrabInput().attach(*this);
	renderSettings.getPointerHideDelay().attach(*this);
	renderSettings.getFullScreen().attach(*this);
	eventDistributor.registerEventListener(
		OPENMSX_POINTER_TIMER_EVENT, *this);
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
	// try default bpp
	SDL_Surface* surface = SDL_SetVideoMode(width, height, 0, flags);
	int bytepp = (surface ? surface->format->BytesPerPixel : 0);
	if (bytepp != 2 && bytepp != 4) {
		surface = NULL;
	}
#if !HAVE_16BPP
	if (bytepp == 2) {
		surface = NULL;
	}
#endif
#if !HAVE_32BPP
	if (bytepp == 4) {
		surface = NULL;
	}
#endif
	// try supported bpp in order of preference
#if HAVE_16BPP
	if (!surface) surface = SDL_SetVideoMode(width, height, 15, flags);
	if (!surface) surface = SDL_SetVideoMode(width, height, 16, flags);
#endif
#if HAVE_32BPP
	if (!surface) surface = SDL_SetVideoMode(width, height, 32, flags);
#endif

	if (!surface) {
		std::string err = SDL_GetError();
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		throw InitException("Could not open any screen: " + err);
	}
	setSDLDisplaySurface(surface);

	// on SDL-GP2X we need to re-hide the mouse after SDL_SetVideoMode()
	SDL_ShowCursor(SDL_DISABLE);

#ifdef _WIN32
	// find our current location...
	HWND handle = GetActiveWindow();
	RECT windowRect;
	GetWindowRect(handle, &windowRect);
	// ...and adjust if needed
	// HWND_TOP is #defined as ((HWND)0)
	HWND OPENMSX_HWND_TOP = static_cast<HWND>(0);
	if ((windowRect.right < 0) || (windowRect.bottom < 0)) {
		SetWindowPos(handle, OPENMSX_HWND_TOP, lastWindowX, lastWindowY,
		             0, 0, SWP_NOSIZE);
	}
#endif
}

VisibleSurface::~VisibleSurface()
{
	prepareDelete();
	eventDistributor.unregisterEventListener(
		OPENMSX_POINTER_TIMER_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_MOTION_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(
		OPENMSX_MOUSE_BUTTON_UP_EVENT, *this);
	inputEventGenerator.getGrabInput().detach(*this);
	renderSettings.getPointerHideDelay().detach(*this);
	renderSettings.getFullScreen().detach(*this);

#ifdef _WIN32
	// Find our current location.
	SDL_Surface* surface = getSDLDisplaySurface();
	if (surface && ((surface->flags & SDL_FULLSCREEN) == 0)) {
		HWND handle = GetActiveWindow();
		RECT windowRect;
		GetWindowRect(handle, &windowRect);
		lastWindowX = windowRect.left;
		lastWindowY = windowRect.top;
	}
#endif
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void VisibleSurface::setWindowTitle(const std::string& title)
{
	SDL_WM_SetCaption(title.c_str(), 0);
}

bool VisibleSurface::setFullScreen(bool wantedState)
{
	SDL_Surface* surface = getSDLDisplaySurface();
	bool currentState = (surface->flags & SDL_FULLSCREEN) != 0;
	if (currentState == wantedState) {
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
	SDL_WM_ToggleFullScreen(surface);
	bool newState = (surface->flags & SDL_FULLSCREEN) != 0;
	return newState == wantedState;
	*/
}

void VisibleSurface::update(const Setting& /*setting*/)
{
	updateCursor();
}

bool VisibleSurface::signalEvent(shared_ptr<const Event> event)
{
	EventType type = event->getType();
	if (type == OPENMSX_POINTER_TIMER_EVENT) {
		// timer expired, hide cursor
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		// mouse action
		assert((type == OPENMSX_MOUSE_MOTION_EVENT) ||
		       (type == OPENMSX_MOUSE_BUTTON_UP_EVENT) ||
		       (type == OPENMSX_MOUSE_BUTTON_DOWN_EVENT));
		updateCursor();
	}
	return true;
}

void VisibleSurface::updateCursor()
{
	cancel();
	if (renderSettings.getFullScreen().getValue() ||
			inputEventGenerator.getGrabInput().getValue()) {
		// always hide cursor in fullscreen or grabinput mode
		SDL_ShowCursor(SDL_DISABLE);
		return;
	}
	double delay = renderSettings.getPointerHideDelay().getValue();
	if (delay == 0.0) {
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_ShowCursor(SDL_ENABLE);
		if (delay > 0.0) {
			schedule(int(delay * 1e6)); // delay in s, schedule in us
		}
	}
}

bool VisibleSurface::alarm()
{
	// this runs in a different thread,
	// so we can't directly do our tricks here
	eventDistributor.distributeEvent(
		new SimpleEvent(OPENMSX_POINTER_TIMER_EVENT));
	return false; // don't repeat alarm
}

} // namespace openmsx
