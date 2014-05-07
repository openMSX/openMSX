#ifdef _WIN32

#include "sdlwin32.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include <SDL.h>
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN // For <SDL_syswm.h>, which defines it carelessly
#endif
#include <SDL_syswm.h>

namespace openmsx {

HWND getSDLWindowHandle()
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if (SDL_GetWMInfo(&info) != 1) {
		throw MSXException(StringOp::Builder() <<
			"SDL_GetWMInfo failed: " << SDL_GetError());
	}
	return info.window;
}

} // namespace openmsx

#endif
