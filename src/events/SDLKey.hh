#ifndef SDLKEY_HH
#define SDLKEY_HH

#include "zstring_view.hh"

#include <SDL3/SDL.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// SDL has no keycode, and thus no SDLK_ constant, for these Japanese keys. We
// use the keycode derived from their scancode instead, see
// SDLKey::keycodeOrFromScancode().
#if defined(SDLK_HENKAN) || defined(SDLK_MUHENKAN)
#error "SDL now defines SDLK_HENKAN or SDLK_MUHENKAN: use those instead of our own definitions below, and check whether SDLKey::keycodeOrFromScancode() is still needed"
#endif
#define SDLK_HENKAN   SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_INTERNATIONAL4)
#define SDLK_MUHENKAN SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_INTERNATIONAL5)

namespace openmsx {

// The combination of:
//   SDL_Scancode, SDL_Keycode and SDL_Keymod: to indicate a specific key and modifiers
//   bool:                                     to indicate up/down
struct SDLKey {
	SDL_Scancode scancode;
	SDL_Keycode keycode;
	SDL_Keymod mod;
	bool down; // down=press / up=release

	// SDL has no keycode for some keys (e.g. the Japanese Henkan key). For
	// those, use a keycode derived from the scancode, so that these keys can
	// still be bound or used as host key.
	[[nodiscard]] static SDL_Keycode keycodeOrFromScancode(SDL_Keycode keycode, SDL_Scancode scancode) {
		return ((keycode == SDLK_UNKNOWN) && (scancode != SDL_SCANCODE_UNKNOWN))
		     ? SDL_Keycode(SDL_SCANCODE_TO_KEYCODE(scancode))
		     : keycode;
	}

	[[nodiscard]] static SDLKey create(SDL_Keycode code, SDL_Scancode scancode, bool down, SDL_Keymod mod = SDL_KMOD_NONE) {
		// scancode is needed for POSITIONAL mapping mode
		return {.scancode = scancode, .keycode = code, .mod = mod, .down = down};
	}

	[[nodiscard]] static SDLKey createDown(SDL_Keycode code, SDL_Scancode scancode) {
		return create(code, scancode, true);
	}

	[[nodiscard]] static std::optional<SDLKey> fromString(std::string_view name);
	[[nodiscard]] static SDL_Keycode keycodeFromString(zstring_view name);

	// only uses the 'keycode', 'mod' and 'down' fields (ignores 'scancode')
	[[nodiscard]] std::string toString() const;
	[[nodiscard]] static std::string toString(SDL_Keycode code);
};

} // namespace openmsx

#endif
