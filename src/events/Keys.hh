// $Id$

#ifndef __KEYS_HH__
#define __KEYS_HH__

#include <SDL/SDL_keysym.h>
#include <map>
#include <string>


class Keys {
	public:
		enum KeyCode {
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
			K_a		= SDLK_a,
			K_b		= SDLK_b,
			K_c		= SDLK_c,
			K_d		= SDLK_d,
			K_e		= SDLK_e,
			K_f		= SDLK_f,
			K_g		= SDLK_g,
			K_h		= SDLK_h,
			K_i		= SDLK_i,
			K_j		= SDLK_j,
			K_k		= SDLK_k,
			K_l		= SDLK_l,
			K_m		= SDLK_m,
			K_n		= SDLK_n,
			K_o		= SDLK_o,
			K_p		= SDLK_p,
			K_q		= SDLK_q,
			K_r		= SDLK_r,
			K_s		= SDLK_s,
			K_t		= SDLK_t,
			K_u		= SDLK_u,
			K_v		= SDLK_v,
			K_w		= SDLK_w,
			K_x		= SDLK_x,
			K_y		= SDLK_y,
			K_z		= SDLK_z,
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
		};
		
		static Keys::KeyCode getCode(const std::string &name);
		static const std::string &getName(const KeyCode keyCode);

	private:
		static void initialize();
	
		static bool init;
		struct ltstr {
			bool operator()(const std::string &s1, const std::string &s2) const {
				return strcasecmp(s1.c_str(), s2.c_str()) < 0;
			}
		};
		static std::map<const std::string, KeyCode, ltstr> keymap;
		static const std::string unknown;
};

#endif
