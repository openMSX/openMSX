#ifndef WIN32_WINDOW_HANDLE_HH
#define WIN32_WINDOW_HANDLE_HH

#ifdef _WIN32

#include <windows.h>

namespace openmsx {

HWND getWindowHandle();

} // namespace openmsx

#endif

#endif // WIN32_WINDOW_HANDLE_HH
