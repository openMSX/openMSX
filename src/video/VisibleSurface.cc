// $Id$

#include "VisibleSurface.hh"
#include "InitException.hh"
#include "Icon.hh"
#include "RenderSettings.hh"
#include "SDLSurfacePtr.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "AlarmEvent.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "InputEventGenerator.hh"
#include "StringOp.hh"
#include "memory.hh"
#include "build-info.hh"
#include "openmsx.hh"

#if PLATFORM_MAEMO5
#include <SDL_syswm.h>
#define _NET_WM_STATE_ADD 1
#endif

#ifdef _WIN32
#include <windows.h>
static int lastWindowX = 0;
static int lastWindowY = 0;
#endif

namespace openmsx {

#if PLATFORM_MAEMO5
static void setMaemo5WMHints(bool fullscreen)
{
	if (!fullscreen) {
		// In windowed mode, stick with default settings.
		return;
	}
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	// In SDL 1.2.14, the header states that 0 is returned for all failures,
	// but the implementation returns 0 for not implemented and -1 for invalid
	// version. Reported as bug 957:
	//   http://bugzilla.libsdl.org/show_bug.cgi?id=957
	if (SDL_GetWMInfo(&info) > 0) {
		::Display* dpy = info.info.x11.display;
		Window win = info.info.x11.fswindow;
		if (win) {
			// Tell the SDL event thread not to touch the X server until we are
			// done with it.
			info.info.x11.lock_func();
			// Fetch atoms; create them if necessary.
			Atom wmStateAtom =
				XInternAtom(dpy, "_NET_WM_STATE", False);
			Atom fullscreenAtom =
				XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
			Atom nonCompositedAtom =
				XInternAtom(dpy, "_HILDON_NON_COMPOSITED_WINDOW", False);
			// Unmap window, so we can remap it when properly configured.
			XUnmapWindow(dpy, win);
			// Tell window manager that we are running fullscreen.
			// TODO: Wouldn't it be better if SDL did this?
			XChangeProperty(
				dpy, win, wmStateAtom, XA_ATOM, 32, PropModeReplace,
				(unsigned char *)&fullscreenAtom, 1
				);
			// Disable compositor to improve painting performance.
			int one = 1;
			XChangeProperty(
				dpy, win, nonCompositedAtom, XA_INTEGER, 32, PropModeReplace,
				(unsigned char *)&one, 1
				);
			// Remap the window with the new settings.
			XMapWindow(dpy, win);
			// Resume the SDL event thread.
			info.info.x11.unlock_func();
		}
	}
}
#endif

VisibleSurface::VisibleSurface(RenderSettings& renderSettings_,
		EventDistributor& eventDistributor_,
		InputEventGenerator& inputEventGenerator_)
	: renderSettings(renderSettings_)
	, eventDistributor(eventDistributor_)
	, inputEventGenerator(inputEventGenerator_)
	, alarm(make_unique<AlarmEvent>(
		eventDistributor, *this, OPENMSX_POINTER_TIMER_EVENT))
{
	if (!SDL_WasInit(SDL_INIT_VIDEO) &&
	    SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		throw InitException(StringOp::Builder() <<
			"SDL video init failed: " << SDL_GetError());
	}

	// set icon
	if (OPENMSX_SET_WINDOW_ICON) {
		SDLSurfacePtr iconSurf(SDL_CreateRGBSurfaceFrom(
			const_cast<char*>(openMSX_icon.pixel_data),
			openMSX_icon.width, openMSX_icon.height,
			openMSX_icon.bytes_per_pixel * 8,
			openMSX_icon.bytes_per_pixel * openMSX_icon.width,
			OPENMSX_BIGENDIAN ? 0xFF000000 : 0x000000FF,
			OPENMSX_BIGENDIAN ? 0x00FF0000 : 0x0000FF00,
			OPENMSX_BIGENDIAN ? 0x0000FF00 : 0x00FF0000,
			OPENMSX_BIGENDIAN ? 0x000000FF : 0xFF000000));
		SDL_SetColorKey(iconSurf.get(), SDL_SRCCOLORKEY, 0);
		SDL_WM_SetIcon(iconSurf.get(), nullptr);
	}

	inputEventGenerator_.getGrabInput().attach(*this);
	renderSettings.getPointerHideDelay().attach(*this);
	renderSettings.getFullScreen().attach(*this);
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
		surface = nullptr;
	}
#if !HAVE_16BPP
	if (bytepp == 2) {
		surface = nullptr;
	}
#endif
#if !HAVE_32BPP
	if (bytepp == 4) {
		surface = nullptr;
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

#if PLATFORM_MAEMO5
	setMaemo5WMHints(flags & SDL_FULLSCREEN);
#endif

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
	SDL_WM_SetCaption(title.c_str(), nullptr);
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

int VisibleSurface::signalEvent(const std::shared_ptr<const Event>& event)
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
	return 0;
}

void VisibleSurface::updateCursor()
{
	alarm->cancel();
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
			alarm->schedule(int(delay * 1e6)); // delay in s, schedule in us
		}
	}
}

} // namespace openmsx
