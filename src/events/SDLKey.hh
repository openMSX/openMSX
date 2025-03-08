#ifndef SDLKEY_HH
#define SDLKEY_HH

#include "zstring_view.hh"

#include <SDL3/SDL.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace openmsx {

// The combination of:
//   SDL_Keycode: to indicate a specific key (key+scancode) and modifiers
//   bool:        to indicate up/down
struct SDLKey {
	SDL_Scancode scancode;
	SDL_Keycode keycode;
	SDL_Keymod mod;
	bool down; // down=press / up=release

	[[nodiscard]] static SDLKey create(SDL_Keycode keycode, bool down, SDL_Keymod mod = SDL_KMOD_NONE) {
		return {.scancode = SDL_SCANCODE_UNKNOWN, .keycode = keycode, .mod = mod, .down = down};
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
