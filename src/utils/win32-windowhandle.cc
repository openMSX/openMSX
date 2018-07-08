#ifdef _WIN32

#include "win32-windowhandle.hh"
#include "MSXException.hh"
#include <SDL.h>
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN // For <SDL_syswm.h>, which defines it carelessly
#endif
#include <SDL_syswm.h>

namespace openmsx {

HWND getWindowHandle()
{
	// There is no window handle until the video subsystem has been initialized.
	if (!SDL_WasInit(SDL_INIT_VIDEO)) {
		SDL_InitSubSystem(SDL_INIT_VIDEO);
	}

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (SDL_GetWMInfo(&info) != 1) {
		throw MSXException("SDL_GetWMInfo failed: ", SDL_GetError());
	}
	return info.window;
}

} // namespace openmsx

#endif
