#ifndef SDLKEY_HH
#define SDLKEY_HH

#include "zstring_view.hh"

#include <SDL.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace openmsx {

// The combination of:
//   SDL_Keysym: to indicate a specific key (key+scancode) and modifiers
//   bool:       to indicate up/down
struct SDLKey {
	SDL_Keysym sym;
	bool down; // down=press / up=release

	[[nodiscard]] static SDLKey create(SDL_Keycode code, bool down, uint16_t mod = 0) {
		SDL_Keysym sym;
		sym.scancode = SDL_SCANCODE_UNKNOWN;
		sym.sym = code;
		sym.mod = mod;
		sym.unused = 0; // unicode
		return {sym, down};
	}

	[[nodiscard]] static SDLKey createDown(SDL_Keycode code) {
		return create(code, true);
	}

	[[nodiscard]] static std::optional<SDLKey> fromString(std::string_view name);
	[[nodiscard]] static SDL_Keycode keycodeFromString(zstring_view name);

	// only uses the 'sym.sym', 'sym.mod' and 'down' fields (ignores sym.scancode and 'sym.unicode')
	[[nodiscard]] std::string toString() const;
	[[nodiscard]] static std::string toString(SDL_Keycode code);
};

} // namespace openmsx

#endif
