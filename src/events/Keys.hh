#ifndef KEYS_HH
#define KEYS_HH

#include <SDL_stdinc.h>
#include <SDL_keycode.h>
#include <SDL_scancode.h>
#include <string>
#include <string_view>

namespace openmsx::Keys {

/**
 * Constants that identify keys and key modifiers.
 *
 * There are two special key codes:
 *  - K_NONE    : returned when we do string -> key code
 *                translation and there is no key with given
 *                name. Most likely the keyname was misspelled.
 *  - K_UNKNOWN : this code is returned when a real key was
 *                pressed, but we have no idea which key it is.
 *                Should only happen when the user has some
 *                exotic keyboard. Note that it might be possible
 *                that there are multiple keys that produce this
 *                code.
 */
enum KeyCode {
	K_MASK = 0x1FFFF,

	K_NONE = K_MASK, // make sure K_NONE has no modifiers set
	K_UNKNOWN = 0,
	K_BACKSPACE = 8,
	K_TAB = 9,
	K_CLEAR = 12,
	K_RETURN = 13,
	K_PAUSE = 19,
	K_ESCAPE = 27,
	K_SPACE = ' ',
	K_EXCLAIM = '!',
	K_QUOTEDBL = '"',
	K_HASH = '#',
	K_DOLLAR = '$',
	K_AMPERSAND = '&',
	K_QUOTE = '\'',
	K_LEFTPAREN = '(',
	K_RIGHTPAREN = ')',
	K_ASTERISK = '*',
	K_PLUS = '+',
	K_COMMA = ',',
	K_MINUS = '-',
	K_PERIOD = '.',
	K_SLASH = '/',
	K_0 = '0',
	K_1 = '1',
	K_2 = '2',
	K_3 = '3',
	K_4 = '4',
	K_5 = '5',
	K_6 = '6',
	K_7 = '7',
	K_8 = '8',
	K_9 = '9',
	K_COLON = ':',
	K_SEMICOLON = ';',
	K_LESS = '<',
	K_EQUALS = '=',
	K_GREATER = '>',
	K_QUESTION = '?',
	K_AT = '@',

	K_LEFTBRACKET = '[',
	K_BACKSLASH = '\\',
	K_RIGHTBRACKET = ']',
	K_CARET = '^',
	K_UNDERSCORE = '_',
	K_BACKQUOTE = '`',
	K_A = 'a',
	K_B = 'b',
	K_C = 'c',
	K_D = 'd',
	K_E = 'e',
	K_F = 'f',
	K_G = 'g',
	K_H = 'h',
	K_I = 'i',
	K_J = 'j',
	K_K = 'k',
	K_L = 'l',
	K_M = 'm',
	K_N = 'n',
	K_O = 'o',
	K_P = 'p',
	K_Q = 'q',
	K_R = 'r',
	K_S = 's',
	K_T = 't',
	K_U = 'u',
	K_V = 'v',
	K_W = 'w',
	K_X = 'x',
	K_Y = 'y',
	K_Z = 'z',
	K_DELETE = 127,

	// Numeric keypad
	K_KP0 = 0x100,
	K_KP1 = 0x101,
	K_KP2 = 0x102,
	K_KP3 = 0x103,
	K_KP4 = 0x104,
	K_KP5 = 0x105,
	K_KP6 = 0x106,
	K_KP7 = 0x107,
	K_KP8 = 0x108,
	K_KP9 = 0x109,
	K_KP_PERIOD = 0x10A,
	K_KP_DIVIDE = 0x10B,
	K_KP_MULTIPLY = 0x10C,
	K_KP_MINUS = 0x10D,
	K_KP_PLUS = 0x10E,
	K_KP_ENTER = 0x10F,
	K_KP_EQUALS = 0x110,

	// Arrows + Home/End pad
	K_UP = 0x111,
	K_DOWN = 0x112,
	K_RIGHT = 0x113,
	K_LEFT = 0x114,
	K_INSERT = 0x115,
	K_HOME = 0x116,
	K_END = 0x117,
	K_PAGEUP = 0x118,
	K_PAGEDOWN = 0x119,

	// Function keys
	K_F1 = 0x11A,
	K_F2 = 0x11B,
	K_F3 = 0x11C,
	K_F4 = 0x11D,
	K_F5 = 0x11E,
	K_F6 = 0x11F,
	K_F7 = 0x120,
	K_F8 = 0x121,
	K_F9 = 0x122,
	K_F10 = 0x123,
	K_F11 = 0x124,
	K_F12 = 0x125,
	K_F13 = 0x126,
	K_F14 = 0x127,
	K_F15 = 0x128,
	K_F16 = 0x129,
	K_F17 = 0x12A,
	K_F18 = 0x12B,
	K_F19 = 0x144, // NOTE: continuing at 0x144
	K_F20 = 0x145,
	K_F21 = 0x146,
	K_F22 = 0x147,
	K_F23 = 0x148,
	K_F24 = 0x149,

	// Key state modifier keys
	K_NUMLOCK = 0x12C, // NOTE: here we continue at 0x12C
	K_CAPSLOCK = 0x12D,
	K_SCROLLLOCK = 0x12E,
	K_RSHIFT = 0x12F,
	K_LSHIFT = 0x130,
	K_RCTRL = 0x131,
	K_LCTRL = 0x132,
	K_RALT = 0x133,
	K_LALT = 0x134,
//	K_RMETA = 0x135,
//	K_LMETA = 0x136,
	K_LSUPER = 0x137,  // Left "Windows" key
	K_RSUPER = 0x138,  // Right "Windows" key
	K_MODE = 0x139,    // "Alt Gr" key
//	K_COMPOSE = 0x13A, // Multi-key compose key

	// Miscellaneous function keys
	K_HELP = 0x13B,
	K_PRINT = 0x13C,
	K_SYSREQ = 0x13D,
//	K_BREAK = 0x13E,
	K_MENU = 0x13F,
	K_POWER = 0x140,   // Power Macintosh power key
//	K_EURO = 0x141,    // Some european keyboards
	K_UNDO = 0x142,

	// Application Control keys
	K_BACK = 0x143,

	// Some japanese keyboard keys are unknown to SDL.
	// That is; they are all mapped to SDL_Keycode=0
	// However, they can be recognized on their scancode
	// These keys are useful for Japanese users who want to map
	// their host keyboard to the Japanese MSX keyboard
	// (e.g. the MSX turbo R keyboard)
	// Define some codes above suspected SDL_Keycode value range, to
	// avoid clash with SDL_Keycode values
	K_ZENKAKU_HENKAKU   = 0x10000, // Enables EMI mode (MSX does this with CTRL+SPACE)
	K_MUHENKAN          = 0x10001, // ???
	K_HENKAN_MODE       = 0x10002, // Similar to kanalock on MSX
	K_HIRAGANA_KATAKANA = 0x10003, // MSX switches between the two sets based on capslock state

	// Modifiers
	KM_SHIFT    = 0x020000,
	KM_CTRL     = 0x040000,
	KM_ALT      = 0x080000,
	KM_META     = 0x100000,
	KM_MODE     = 0x200000,

	// Direction modifiers
	KD_PRESS    = 0,        // key press
	KD_RELEASE  = 0x400000 // key release
};

/**
 * Translate key name to key code.
 * Returns K_NONE when the name is unknown.
 */
[[nodiscard]] KeyCode getCode(std::string_view name);

/** Translate SDL_Keycode/SDL_Scancode into openMSX key/scan Keycode's. */
[[nodiscard]] std::pair<KeyCode, KeyCode> getCodes(
	SDL_Keycode keyCode, Uint16 mod = KMOD_NONE, SDL_Scancode scanCode = SDL_SCANCODE_UNKNOWN, bool release = false);

/**
 * Translate key code to key name.
 * Returns the string "unknown" for unknown key codes.
 */
[[nodiscard]] std::string getName(KeyCode keyCode);

/**
 * Convenience method to create key combinations (hides ugly casts).
 */
[[nodiscard]] inline KeyCode combine(KeyCode key, KeyCode modifier) {
	return static_cast<KeyCode>(int(key) | int(modifier));
}

} // namespace openmsx::Keys

#endif
