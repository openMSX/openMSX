// $Id$

#ifndef __KEYS_HH__
#define __KEYS_HH__

#include <SDL/SDL_keysym.h>
#include <map>
#include <string>

using std::map;
using std::string;


namespace openmsx {

class Keys {
public:
	// There are two special key codes:
	//  - K_NONE    : returned when we do string -> key code
	//                translation and there is no key with given
	//                name. Most likely the keyname was misspelled.
	//  - K_UNKNOWN : this code is returned when a real key was
	//                pressed, but we have no idea which key it is.
	//                Should only happen when the user has some
	//                exotic keyboard. Note that it might be possible
	//                that there are multiple keys that produce this
	//                code.
	enum KeyCode 
	{
		K_NONE          = -1,
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

		K_MASK		= 0xFFFF,
		
		// Modifiers
		KM_SHIFT	= 0x10000,
		KM_CTRL		= 0x20000,
		KM_ALT		= 0x40000,
		KM_META		= 0x80000,
		
		// Direction modifiers
		KD_DOWN		= 0,		// key press
		KD_UP		= 0x100000,	// key release
	};

	/**
	 * Translate key name to key code.
	 * Returns K_NONE when the name is unknown.
	 */
	static KeyCode getCode(const string& name);
	static KeyCode getCode(SDLKey key, SDLMod mod = KMOD_NONE, bool up = false);

	/**
	 * Translate key code to key name.
	 * Returns the string "unknown" for unknown key codes.
	 */
	static const string getName(KeyCode keyCode);

private:
	static void initialize();

	struct ltstrcase {
		bool operator()(const string& s1, const string& s2) const {
			return strcasecmp(s1.c_str(), s2.c_str()) < 0;
		}
	};
	typedef map<string, KeyCode, ltstrcase> KeyMap;
	static KeyMap keymap;
};

} // namespace openmsx

#endif // __KEYS_HH__
