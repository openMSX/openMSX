#ifndef SDLKEY_HH
#define SDLKEY_HH

#include <SDL.h>
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

	[[nodiscard]] static SDLKey create(SDL_Keycode code, bool down) {
		SDL_Keysym sym;
		sym.scancode = SDL_SCANCODE_UNKNOWN;
		sym.sym = code;
		sym.mod = 0;
		sym.unused = 0; // unicode
		return {sym, down};
	}

	[[nodiscard]] static SDLKey createDown(SDL_Keycode code) {
		return create(code, true);
	}

	[[nodiscard]] static std::optional<SDLKey> fromString(std::string_view name);

	// only uses the 'sym.sym', 'sym.mod' and 'down' fields (ignores sym.scancode and 'sym.unicode')
	[[nodiscard]] std::string toString() const;
};

} // namespace openmsx

#endif
