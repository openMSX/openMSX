#ifdef _WIN32

#include "AltSpaceSuppressor.hh"
#include "MSXException.hh"
#include "openmsx.hh"
#include <cassert>

// This module exists to fix the following bug:
// [ 1206036 ] Windows Control Menu while using ALT
// http://sourceforge.net/tracker/index.php?func=detail&aid=1206036&group_id=38274&atid=421861
//
// The Windows control menu will pop up by default in a window when the user
// presses LALT+SPACE or RALT+SPACE.  Programmatically, this shows up to the
// application as a series of Windows messages: the respective WM_SYSKEYDOWN
// events for ALT and SPACE, followed by a WM_SYSCOMMAND message whose default
// processing by the OS results in the control menu itself appearing.
//
// The fix, at the application level, is to handle that WM_SYSCOMMAND message
// (effectively swallowing it) and thus to prevent it from being forwarded to
// Win32's DefWindowProc.
//
// We first attempted to fix this by interacting with SDL's messaging
// subsystem. The hope was that by calling SDL_SetEventFilter with a custom
// event filter, we could intercept and silence the offending WM_SYSCOMMAND
// message. Unfortunately, SDL allows an application listening for
// SDL_SYSWMEVENT events to see those messages, but it does _not_ allow the
// application to indicate that the event was handled and should not be
// processed further.
//
// More concretely, the SDL code as of 1.2.13 in SDL_dibevents.c (circa line
// 250) ignores the return value from the event filter for WM events and
// ultimately forwards the message to DefWindowProc anyway.  Further, in
// SDL_dx5events.c line 532, a comment implies that this is by design:
//
//         It would be better to allow the application to
//         decide whether or not to blow these off, but the
//         semantics of SDL_PrivateSysWMEvent() don't allow
//         the application that choice.
//
// We filed bug 686 (http://bugzilla.libsdl.org/show_bug.cgi?id=686) on the SDL
// developers. If they respond positively, we may want to return to a solution
// along those lines.
//
// Our second attempt at a fix, the one represented by the code below, involves
// registering our own Windows proc, filtering out the offending WM_SYSCOMMAND
// event, and letting the rest go through.
//
// This is currently integrated into openmsx inside the SDLVideoSystem class.

namespace openmsx {

WindowLongPtrStacker::WindowLongPtrStacker(int index, LONG_PTR value)
	: hWnd(nullptr)
	, nIndex(index)
	, newValue(value)
{
}

void WindowLongPtrStacker::Push(HWND hWndArg)
{
	assert(hWndArg);
	hWnd = hWndArg;

	SetLastError(0);
	oldValue = SetWindowLongPtr(hWnd, nIndex, newValue);
	if (!oldValue && GetLastError()) {
		throw MSXException("SetWindowLongPtr failed");
	}
}

void WindowLongPtrStacker::Pop()
{
	assert(hWnd);
	if (oldValue) {
		SetLastError(0);
		LONG_PTR removedValue =	SetWindowLongPtr(hWnd, nIndex, oldValue);
		if (!removedValue && GetLastError()) {
			throw MSXException("SetWindowLongPtr failed");
		}
		assert(removedValue	== newValue);
	}
}

LONG_PTR WindowLongPtrStacker::GetOldValue()
{
	return oldValue;
}

WindowLongPtrStacker AltSpaceSuppressor::procStacker(
	GWLP_WNDPROC,
	reinterpret_cast<LONG_PTR>(InterceptorWndProc));

void AltSpaceSuppressor::Start(HWND hWnd)
{
	procStacker.Push(hWnd);
}

void AltSpaceSuppressor::Stop()
{
	procStacker.Pop();
}

LRESULT AltSpaceSuppressor::InterceptorWndProc(
	HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT lResult = 0;
	if (SuppressAltSpace(hWnd, message, wParam, lParam, &lResult)) {
		// Message processed
		return lResult;
	}

	// Message not processed, use default handling
	auto nextWndProc = reinterpret_cast<WNDPROC>(procStacker.GetOldValue());
	if (nextWndProc) {
		// Forward to the window proc we replaced
		return CallWindowProc(nextWndProc, hWnd, message, wParam, lParam);
	}

	// Let the system default window proc handle the message
	return DefWindowProc(hWnd, message, wParam, lParam);
}

bool AltSpaceSuppressor::SuppressAltSpace(
	HWND /*hWnd*/, UINT message, WPARAM wParam, LPARAM lParam,
	LRESULT* outResult)
{
	if (message == WM_SYSCOMMAND &&
	    wParam == SC_KEYMENU &&
	    lParam == LPARAM(' ')) {
		// Suppressed ALT+SPACE message
		*outResult = 0;
		return true; // processed
	}
	return false; // not processed
}

} // namespace openmsx

#endif
