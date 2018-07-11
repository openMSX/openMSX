#ifndef KEYS_HH
#define KEYS_HH

#include "string_view.hh"
#include <SDL_stdinc.h>
#include <SDL_keysym.h> // TODO
#include <string>

namespace openmsx {
namespace Keys {

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
	K_NONE		= -1,
	K_UNKNOWN	= SDLK_UNKNOWN,
	K_BACKSPACE	= SDLK_BACKSPACE,
	K_TAB		= SDLK_TAB,
	K_CLEAR		= SDLK_CLEAR,
	K_RETURN	= SDLK_RETURN,
	K_PAUSE		= SDLK_PAUSE,
	K_ESCAPE	= SDLK_ESCAPE,
	K_SPACE		= SDLK_SPACE,
	K_EXCLAIM	= SDLK_EXCLAIM,
	K_QUOTEDBL	= SDLK_QUOTEDBL,
	K_HASH		= SDLK_HASH,
	K_DOLLAR	= SDLK_DOLLAR,
	K_AMPERSAND	= SDLK_AMPERSAND,
	K_QUOTE		= SDLK_QUOTE,
	K_LEFTPAREN	= SDLK_LEFTPAREN,
	K_RIGHTPAREN	= SDLK_RIGHTPAREN,
	K_ASTERISK	= SDLK_ASTERISK,
	K_PLUS		= SDLK_PLUS,
	K_COMMA		= SDLK_COMMA,
	K_MINUS		= SDLK_MINUS,
	K_PERIOD	= SDLK_PERIOD,
	K_SLASH		= SDLK_SLASH,
	K_0		= SDLK_0,
	K_1		= SDLK_1,
	K_2		= SDLK_2,
	K_3		= SDLK_3,
	K_4		= SDLK_4,
	K_5		= SDLK_5,
	K_6		= SDLK_6,
	K_7		= SDLK_7,
	K_8		= SDLK_8,
	K_9		= SDLK_9,
	K_COLON		= SDLK_COLON,
	K_SEMICOLON	= SDLK_SEMICOLON,
	K_LESS		= SDLK_LESS,
	K_EQUALS	= SDLK_EQUALS,
	K_GREATER	= SDLK_GREATER,
	K_QUESTION	= SDLK_QUESTION,
	K_AT		= SDLK_AT,

	K_LEFTBRACKET	= SDLK_LEFTBRACKET,
	K_BACKSLASH	= SDLK_BACKSLASH,
	K_RIGHTBRACKET	= SDLK_RIGHTBRACKET,
	K_CARET		= SDLK_CARET,
	K_UNDERSCORE	= SDLK_UNDERSCORE,
	K_BACKQUOTE	= SDLK_BACKQUOTE,
	K_A		= SDLK_a,
	K_B		= SDLK_b,
	K_C		= SDLK_c,
	K_D		= SDLK_d,
	K_E		= SDLK_e,
	K_F		= SDLK_f,
	K_G		= SDLK_g,
	K_H		= SDLK_h,
	K_I		= SDLK_i,
	K_J		= SDLK_j,
	K_K		= SDLK_k,
	K_L		= SDLK_l,
	K_M		= SDLK_m,
	K_N		= SDLK_n,
	K_O		= SDLK_o,
	K_P		= SDLK_p,
	K_Q		= SDLK_q,
	K_R		= SDLK_r,
	K_S		= SDLK_s,
	K_T		= SDLK_t,
	K_U		= SDLK_u,
	K_V		= SDLK_v,
	K_W		= SDLK_w,
	K_X		= SDLK_x,
	K_Y		= SDLK_y,
	K_Z		= SDLK_z,
	K_DELETE	= SDLK_DELETE,

	K_WORLD_0	= SDLK_WORLD_0,
	K_WORLD_1	= SDLK_WORLD_1,
	K_WORLD_2	= SDLK_WORLD_2,
	K_WORLD_3	= SDLK_WORLD_3,
	K_WORLD_4	= SDLK_WORLD_4,
	K_WORLD_5	= SDLK_WORLD_5,
	K_WORLD_6	= SDLK_WORLD_6,
	K_WORLD_7	= SDLK_WORLD_7,
	K_WORLD_8	= SDLK_WORLD_8,
	K_WORLD_9	= SDLK_WORLD_9,
	K_WORLD_10	= SDLK_WORLD_10,
	K_WORLD_11	= SDLK_WORLD_11,
	K_WORLD_12	= SDLK_WORLD_12,
	K_WORLD_13	= SDLK_WORLD_13,
	K_WORLD_14	= SDLK_WORLD_14,
	K_WORLD_15	= SDLK_WORLD_15,
	K_WORLD_16	= SDLK_WORLD_16,
	K_WORLD_17	= SDLK_WORLD_17,
	K_WORLD_18	= SDLK_WORLD_18,
	K_WORLD_19	= SDLK_WORLD_19,
	K_WORLD_20	= SDLK_WORLD_20,
	K_WORLD_21	= SDLK_WORLD_21,
	K_WORLD_22	= SDLK_WORLD_22,
	K_WORLD_23	= SDLK_WORLD_23,
	K_WORLD_24	= SDLK_WORLD_24,
	K_WORLD_25	= SDLK_WORLD_25,
	K_WORLD_26	= SDLK_WORLD_26,
	K_WORLD_27	= SDLK_WORLD_27,
	K_WORLD_28	= SDLK_WORLD_28,
	K_WORLD_29	= SDLK_WORLD_29,
	K_WORLD_30	= SDLK_WORLD_30,
	K_WORLD_31	= SDLK_WORLD_31,
	K_WORLD_32	= SDLK_WORLD_32,
	K_WORLD_33	= SDLK_WORLD_33,
	K_WORLD_34	= SDLK_WORLD_34,
	K_WORLD_35	= SDLK_WORLD_35,
	K_WORLD_36	= SDLK_WORLD_36,
	K_WORLD_37	= SDLK_WORLD_37,
	K_WORLD_38	= SDLK_WORLD_38,
	K_WORLD_39	= SDLK_WORLD_39,
	K_WORLD_40	= SDLK_WORLD_40,
	K_WORLD_41	= SDLK_WORLD_41,
	K_WORLD_42	= SDLK_WORLD_42,
	K_WORLD_43	= SDLK_WORLD_43,
	K_WORLD_44	= SDLK_WORLD_44,
	K_WORLD_45	= SDLK_WORLD_45,
	K_WORLD_46	= SDLK_WORLD_46,
	K_WORLD_47	= SDLK_WORLD_47,
	K_WORLD_48	= SDLK_WORLD_48,
	K_WORLD_49	= SDLK_WORLD_49,
	K_WORLD_50	= SDLK_WORLD_50,
	K_WORLD_51	= SDLK_WORLD_51,
	K_WORLD_52	= SDLK_WORLD_52,
	K_WORLD_53	= SDLK_WORLD_53,
	K_WORLD_54	= SDLK_WORLD_54,
	K_WORLD_55	= SDLK_WORLD_55,
	K_WORLD_56	= SDLK_WORLD_56,
	K_WORLD_57	= SDLK_WORLD_57,
	K_WORLD_58	= SDLK_WORLD_58,
	K_WORLD_59	= SDLK_WORLD_59,
	K_WORLD_60	= SDLK_WORLD_60,
	K_WORLD_61	= SDLK_WORLD_61,
	K_WORLD_62	= SDLK_WORLD_62,
	K_WORLD_63	= SDLK_WORLD_63,
	K_WORLD_64	= SDLK_WORLD_64,
	K_WORLD_65	= SDLK_WORLD_65,
	K_WORLD_66	= SDLK_WORLD_66,
	K_WORLD_67	= SDLK_WORLD_67,
	K_WORLD_68	= SDLK_WORLD_68,
	K_WORLD_69	= SDLK_WORLD_69,
	K_WORLD_70	= SDLK_WORLD_70,
	K_WORLD_71	= SDLK_WORLD_71,
	K_WORLD_72	= SDLK_WORLD_72,
	K_WORLD_73	= SDLK_WORLD_73,
	K_WORLD_74	= SDLK_WORLD_74,
	K_WORLD_75	= SDLK_WORLD_75,
	K_WORLD_76	= SDLK_WORLD_76,
	K_WORLD_77	= SDLK_WORLD_77,
	K_WORLD_78	= SDLK_WORLD_78,
	K_WORLD_79	= SDLK_WORLD_79,
	K_WORLD_80	= SDLK_WORLD_80,
	K_WORLD_81	= SDLK_WORLD_81,
	K_WORLD_82	= SDLK_WORLD_82,
	K_WORLD_83	= SDLK_WORLD_83,
	K_WORLD_84	= SDLK_WORLD_84,
	K_WORLD_85	= SDLK_WORLD_85,
	K_WORLD_86	= SDLK_WORLD_86,
	K_WORLD_87	= SDLK_WORLD_87,
	K_WORLD_88	= SDLK_WORLD_88,
	K_WORLD_89	= SDLK_WORLD_89,
	K_WORLD_90	= SDLK_WORLD_90,
	K_WORLD_91	= SDLK_WORLD_91,
	K_WORLD_92	= SDLK_WORLD_92,
	K_WORLD_93	= SDLK_WORLD_93,
	K_WORLD_94	= SDLK_WORLD_94,
	K_WORLD_95	= SDLK_WORLD_95,

	// Numeric keypad
	K_KP0		= SDLK_KP0,
	K_KP1		= SDLK_KP1,
	K_KP2		= SDLK_KP2,
	K_KP3		= SDLK_KP3,
	K_KP4		= SDLK_KP4,
	K_KP5		= SDLK_KP5,
	K_KP6		= SDLK_KP6,
	K_KP7		= SDLK_KP7,
	K_KP8		= SDLK_KP8,
	K_KP9		= SDLK_KP9,
	K_KP_PERIOD	= SDLK_KP_PERIOD,
	K_KP_DIVIDE	= SDLK_KP_DIVIDE,
	K_KP_MULTIPLY	= SDLK_KP_MULTIPLY,
	K_KP_MINUS	= SDLK_KP_MINUS,
	K_KP_PLUS	= SDLK_KP_PLUS,
	K_KP_ENTER	= SDLK_KP_ENTER,
	K_KP_EQUALS	= SDLK_KP_EQUALS,

	// Arrows + Home/End pad
	K_UP		= SDLK_UP,
	K_DOWN		= SDLK_DOWN,
	K_RIGHT		= SDLK_RIGHT,
	K_LEFT		= SDLK_LEFT,
	K_INSERT	= SDLK_INSERT,
	K_HOME		= SDLK_HOME,
	K_END		= SDLK_END,
	K_PAGEUP	= SDLK_PAGEUP,
	K_PAGEDOWN	= SDLK_PAGEDOWN,

	// Function keys
	K_F1		= SDLK_F1,
	K_F2		= SDLK_F2,
	K_F3		= SDLK_F3,
	K_F4		= SDLK_F4,
	K_F5		= SDLK_F5,
	K_F6		= SDLK_F6,
	K_F7		= SDLK_F7,
	K_F8		= SDLK_F8,
	K_F9		= SDLK_F9,
	K_F10		= SDLK_F10,
	K_F11		= SDLK_F11,
	K_F12		= SDLK_F12,
	K_F13		= SDLK_F13,
	K_F14		= SDLK_F14,
	K_F15		= SDLK_F15,

	// Key state modifier keys
	K_NUMLOCK	= SDLK_NUMLOCK,
	K_CAPSLOCK	= SDLK_CAPSLOCK,
	K_SCROLLOCK	= SDLK_SCROLLOCK,
	K_RSHIFT	= SDLK_RSHIFT,
	K_LSHIFT	= SDLK_LSHIFT,
	K_RCTRL		= SDLK_RCTRL,
	K_LCTRL		= SDLK_LCTRL,
	K_RALT		= SDLK_RALT,
	K_LALT		= SDLK_LALT,
	K_RMETA		= SDLK_RMETA,
	K_LMETA		= SDLK_LMETA,
	K_LSUPER	= SDLK_LSUPER,	// Left "Windows" key
	K_RSUPER	= SDLK_RSUPER,	// Right "Windows" key
	K_MODE		= SDLK_MODE,	// "Alt Gr" key
	K_COMPOSE	= SDLK_COMPOSE,	// Multi-key compose key

	// Miscellaneous function keys
	K_HELP		= SDLK_HELP,
	K_PRINT		= SDLK_PRINT,
	K_SYSREQ	= SDLK_SYSREQ,
	K_BREAK		= SDLK_BREAK,
	K_MENU		= SDLK_MENU,
	K_POWER		= SDLK_POWER,	// Power Macintosh power key
	K_EURO		= SDLK_EURO,	// Some european keyboards
	K_UNDO		= SDLK_UNDO,

	// Some japanese keyboard keys are unknown to SDL.
	// That is; they are all mapped to SDLKey=0
	// However, they can recognized on their scancode
	// These keys are usefull for Japanese users who want to map
	// their host keyboard to the Japanese MSX keyboard
	// (e.g. the MSX turbo R keyboard)
	// Define some codes above suspected SDLKey value range, to
	// avoid clash with SDLKey values
	K_ZENKAKU_HENKAKU   = 0x10000, // Enables EMI mode (MSX does this with CTRL+SPACE)
	K_MUHENKAN          = 0x10001, // ???
	K_HENKAN_MODE       = 0x10002, // Similar to kanalock on MSX
	K_HIRAGANA_KATAKANA = 0x10003, // MSX switches between the two sets based on capslock state

	K_MASK		= 0x1FFFF,

	// Modifiers
	KM_SHIFT	= 0x020000,
	KM_CTRL		= 0x040000,
	KM_ALT		= 0x080000,
	KM_META		= 0x100000,
	KM_MODE		= 0x200000,

	// Direction modifiers
	KD_PRESS	= 0,		// key press
	KD_RELEASE	= 0x400000	// key release
};

/**
 * Translate key name to key code.
 * Returns K_NONE when the name is unknown.
 */
KeyCode getCode(string_view name);

KeyCode getCode(SDLKey key, SDLMod mod = KMOD_NONE, Uint8 scancode = 0, bool release = false);

/**
 * Translate key code to key name.
 * Returns the string "unknown" for unknown key codes.
 */
const std::string getName(KeyCode keyCode);

/**
 * Convenience method to create key combinations (hides ugly casts).
 */
inline KeyCode combine(KeyCode key, KeyCode modifier) {
	return static_cast<KeyCode>(int(key) | int(modifier));
}

} // namespace Keys
} // namespace openmsx

#endif
