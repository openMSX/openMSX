#ifdef _WIN32

#include "sdlwin32.hh"
#include "MSXException.hh"
#include <SDL.h>
#include <SDL_syswm.h>
#include <string>

namespace openmsx {

HWND getSDLWindowHandle()
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (!SDL_GetWMInfo(&info)) {
		throw MSXException("SDL_GetWMInfo failed: " + std::string(SDL_GetError()));
	}
	return info.window;
}

} // namespace openmsx

#endif
