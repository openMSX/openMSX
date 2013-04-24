#ifndef SDLWIN32_HH
#define SDLWIN32_HH

#ifdef _WIN32

#include <windows.h>

namespace openmsx {

HWND getSDLWindowHandle();

} // namespace openmsx

#endif

#endif // SDLWIN32_HH
