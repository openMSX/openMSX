#include "SDLKey.hh"

#include "StringOp.hh"
#include "one_of.hh"
#include "ranges.hh"

#include <algorithm>
#include <array>

namespace openmsx {

// We prefer to use 'SDL_GetKeyFromName()' to translate a name into a Keycode.
// That way when new keys are added to SDL2, these immediately work in openMSX.
// In the past we had our own mapping, and to keep backwards compatible (e.g.
// because key-names can end up in settings.xml) we retain (part of) that old
// mapping.
static SDL_Keycode getKeyFromOldOpenmsxName(std::string_view name)
{
	// These (old) openMSX names are the same in SDL2. Except for
	// capitalization (e.g. "PAGEUP" vs "PageUp"), but we anyway
	// do a case-insensitive search.
	// "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	// "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
	// "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
	// "RETURN", "ESCAPE", "BACKSPACE", "TAB", "SPACE", "CAPSLOCK",
	// "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9",
	// "F10", "F11", "F12", "F13", "F14", "F15", "F16", "F17",
	// "F18", "F19", "F20", "F21", "F22", "F23", "F24",
	// "SCROLLLOCK", "PAUSE", "INSERT", "HOME", "PAGEUP", "DELETE",
	// "END", "PAGEDOWN", "RIGHT", "LEFT", "DOWN", "UP", "NUMLOCK",
	// "POWER", "HELP", "MENU", "UNDO", "SYSREQ", "CLEAR",

	// For these (old) openMSX names it's not clear which SDL2 keycode
	// corresponds to it. So, at least for now, these are no longer
	// supported.
	// "BACK", "ZENKAKU_HENKAKU", "MUHENKAN", "HENKAN_MODE", "HIRAGANA_KATAKANA",

	struct M {
		std::string_view name;
		SDL_Keycode code;
	};
	static constexpr std::array unsortedMap = {
		// Old openMSX name, SDL-key-code, new SDL2 name
		M{"EXCLAIM",     SDLK_EXCLAIM},     // !
		M{"QUOTEDBL",    SDLK_DBLAPOSTROPHE}, // "
		M{"HASH",        SDLK_HASH},        // #
		M{"DOLLAR",      SDLK_DOLLAR},      // $
		M{"AMPERSAND",   SDLK_AMPERSAND},   // &
		M{"QUOTE",       SDLK_APOSTROPHE},  // '
		M{"LEFTPAREN",   SDLK_LEFTPAREN},   // (
		M{"RIGHTPAREN",  SDLK_RIGHTPAREN},  // )
		M{"ASTERISK",    SDLK_ASTERISK},    // *
		M{"PLUS",        SDLK_PLUS},        // +
		M{"COMMA",       SDLK_COMMA},       // ,
		M{"MINUS",       SDLK_MINUS},       // -
		M{"PERIOD",      SDLK_PERIOD},      // .
		M{"SLASH",       SDLK_SLASH},       // /
		M{"COLON",       SDLK_COLON},       // :
		M{"SEMICOLON",   SDLK_SEMICOLON},   // ;
		M{"LESS",        SDLK_LESS},        // <
		M{"EQUALS",      SDLK_EQUALS},      // =
		M{"GREATER",     SDLK_GREATER},     // >
		M{"QUESTION",    SDLK_QUESTION},    // ?
		M{"AT",          SDLK_AT},          // @
		M{"LEFTBRACKET", SDLK_LEFTBRACKET}, // [
		M{"BACKSLASH",   SDLK_BACKSLASH},   //
		M{"RIGHTBRACKET",SDLK_RIGHTBRACKET},// ]
		M{"CARET",       SDLK_CARET},       // ^
		M{"UNDERSCORE",  SDLK_UNDERSCORE},  // _
		M{"BACKQUOTE",   SDLK_GRAVE},       // `
		M{"PRINT",       SDLK_PRINTSCREEN}, // PrintScreen
		M{"SCROLLOCK",   SDLK_SCROLLLOCK},  // ScrollLock   backwards compat for typo
		M{"KP_DIVIDE",   SDLK_KP_DIVIDE},   // Keypad /
		M{"KP_MULTIPLY", SDLK_KP_MULTIPLY}, // Keypad *
		M{"KP_MINUS",    SDLK_KP_MINUS},    // Keypad -
		M{"KP_PLUS",     SDLK_KP_PLUS},     // Keypad +
		M{"KP_ENTER",    SDLK_KP_ENTER},    // Keypad Enter
		M{"KP0",         SDLK_KP_0},        // Keypad 0
		M{"KP1",         SDLK_KP_1},        // Keypad 1
		M{"KP2",         SDLK_KP_2},        // Keypad 2
		M{"KP3",         SDLK_KP_3},        // Keypad 3
		M{"KP4",         SDLK_KP_4},        // Keypad 4
		M{"KP5",         SDLK_KP_5},        // Keypad 5
		M{"KP6",         SDLK_KP_6},        // Keypad 6
		M{"KP7",         SDLK_KP_7},        // Keypad 7
		M{"KP8",         SDLK_KP_8},        // Keypad 8
		M{"KP9",         SDLK_KP_9},        // Keypad 9
		M{"KP_PERIOD",   SDLK_KP_PERIOD},   // Keypad .
		M{"KP_EQUALS",   SDLK_KP_EQUALS},   // Keypad =
		M{"LCTRL",       SDLK_LCTRL},       // Left Ctrl
		M{"LSHIFT",      SDLK_LSHIFT},      // Left Shift
		M{"LALT",        SDLK_LALT},        // Left Alt
		M{"LSUPER",      SDLK_LGUI},        // Left GUI     Left "Windows" key
		M{"RCTRL",       SDLK_RCTRL},       // Right Ctrl
		M{"RSHIFT",      SDLK_RSHIFT},      // Right Shift
		M{"RALT",        SDLK_RALT},        // Right Alt
		M{"RSUPER",      SDLK_RGUI},        // Right GUI    Right "Windows" key
		M{"RMODE",       SDLK_MODE},        // ModeSwitch   "Alt Gr" key
	};
	static constexpr auto map = []{
		auto result = unsortedMap;
		std::ranges::sort(result, {}, &M::name);
		return result;
	}();

	if (const auto* p = binary_find(map, name, StringOp::caseless{}, &M::name)) {
		return p->code;
	}
	return SDLK_UNKNOWN;
}

SDL_Keycode SDLKey::keycodeFromString(zstring_view name)
{
	auto result = SDL_GetKeyFromName(name.c_str());
	if (result == SDLK_UNKNOWN) {
		// for backwards compatibility
		result = getKeyFromOldOpenmsxName(name);
	}
	return result;
}

std::optional<SDLKey> SDLKey::fromString(std::string_view name)
{
	// Some remarks:
	//
	// SDL2 offers the following function to convert a string to a
	// SDL_Keycode.
	//     SDL_Keycode SDL_GetKeyFromName(const char* name)
	// This works for all keys known to SDL2. Meaning we don't have to
	// maintain our own string->key mapping in openMSX.
	//
	// In the past we did have our own mapping. And unfortunately our
	// mapping was not exactly the same as the (new) SDL2 mapping. Some
	// examples:
	//   We named the '=' key "EQUALS". The SDL2 name is simply "=".
	//   We named the '1' on the numeric keypad "KP1", the SDL2 name is "Keypad 1".
	//   Our name "RCTRL" is "Right Ctrl" in SDL2.
	//   Our name "ESCAPE" is "Escape" in SDL2 (different capitalization).
	//   ...
	// (IMHO) The SDL2 names are better (and much more complete), but to
	// maintain backwards compatibility we keep a list of (old) names (for
	// some keys).
	//
	// The 'SDL_GetKeyFromName() function' is only concerned with naming a
	// single key. In openMSX we also want to name key combinations like
	// "A+CTRL" (a single key combined with one or more modifiers). Or
	// "A,Release" to distinguish a key-press from a release. This function
	// extends the SDL2 function to also parse these such annotations.
	//
	// One complication of these annotations are the separator characters
	// between the base-key and the annotations. OpenMSX interprets these
	// three characters as separators:
	//    ,   +   /
	// Unfortunately these three characters can also appear in the new SDL2
	// names. For example:
	//    Keypad ,    Keypad +    Keypad /
	//    or simply ,    +    /
	// Luckily these characters only(?) appear as the last character in the
	// base name, so we can extend our parser to correctly recognize stuff
	// like:
	//     ++CTRL    ,+SHIFT   Keypad /,Release    Shift++

	SDLKey result = {};
	result.sym.scancode = SDL_SCANCODE_UNKNOWN;
	result.sym.sym = SDLK_UNKNOWN;
	result.sym.mod = SDL_KMOD_NONE;
	result.sym.unused = 0; // unicode
	result.down = true;

	while (!name.empty()) {
		std::string_view part = [&] {
			auto pos = name.find_first_of(",+/");
			if (pos == std::string_view::npos) {
				// no separator character
				return std::exchange(name, std::string_view{});
			} else if ((pos + 1) == name.size()) {
				// ends with a separator, assume it's part of the name
				return std::exchange(name, std::string_view{});
			} else {
				if (name[pos + 1] == one_of(',', '+', '/')) {
					// next character is also a separator, assume
					// the character at 'pos' is part of the name
					++pos;
				}
				auto r = name.substr(0, pos);
				name.remove_prefix(pos + 1);
				return r;
			}
		}();

		StringOp::casecmp cmp;
		if (cmp(part, "SHIFT")) {
			result.sym.mod |= SDL_KMOD_SHIFT;
		} else if (cmp(part, "CTRL")) {
			result.sym.mod |= SDL_KMOD_CTRL;
		} else if (cmp(part, "ALT")) {
			result.sym.mod |= SDL_KMOD_ALT;
		} else if (cmp(part, "GUI") ||
		           cmp(part, "META")) { // bw-compat
			result.sym.mod |= SDL_KMOD_GUI;
		} else if (cmp(part, "MODE")) {
			result.sym.mod |= SDL_KMOD_MODE;

		} else if (cmp(part, "PRESS")) {
			result.down = true;
		} else if (cmp(part, "RELEASE")) {
			result.down = false;

		} else {
			if (result.sym.sym != SDLK_UNKNOWN) {
				// more than one non-modifier component is not allowed
				return {};
			}
			result.sym.sym = keycodeFromString(std::string(part));
			if (result.sym.sym == SDLK_UNKNOWN) {
				return {};
			}
		}
	}
	if (result.sym.sym == SDLK_UNKNOWN) {
		return {};
	}
	return result;
}

std::string SDLKey::toString(SDL_Keycode code)
{
	std::string result = SDL_GetKeyName(code);
	if (result.empty()) result = "unknown";
	return result;
}

std::string SDLKey::toString() const
{
	std::string result = toString(sym.sym);
	if (sym.mod & SDL_KMOD_CTRL) {
		result += "+CTRL";
	}
	if (sym.mod & SDL_KMOD_SHIFT) {
		result += "+SHIFT";
	}
	if (sym.mod & SDL_KMOD_ALT) {
		result += "+ALT";
	}
	if (sym.mod & SDL_KMOD_GUI) {
		result += "+GUI";
	}
	if (sym.mod & SDL_KMOD_MODE) {
		result += "+MODE";
	}
	if (!down) {
		result += ",RELEASE";
	}
	return result;
}

} // namespace openmsx
