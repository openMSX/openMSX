// $Id$

#include "Keys.hh"


Keys::KeyCode Keys::getCode(const std::string &name)
{
	initialize();
	return keymap[name];
}

const std::string &Keys::getName(const KeyCode keyCode)
{
	static const std::string unknown("unknown");

	initialize();
	std::map<const std::string, KeyCode, ltstr>::const_iterator it;
	for (it = keymap.begin(); it != keymap.end(); it++) {
		if (it->second == keyCode)
			return it->first;
	}
	return unknown;
}

void Keys::initialize()
{
	if (init) return;
	init = true;

	keymap["UNKNOWN"]	= K_UNKNOWN;
	keymap["BACKSPACE"]	= K_BACKSPACE;
	keymap["TAB"]		= K_TAB;
	keymap["CLEAR"]		= K_CLEAR;
	keymap["RETURN"]	= K_RETURN;
	keymap["PAUSE"]		= K_PAUSE;
	keymap["ESCAPE"]	= K_ESCAPE;
	keymap["SPACE"]		= K_SPACE;
	keymap["EXCLAIM"]	= K_EXCLAIM;
	keymap["QUOTEDBL"]	= K_QUOTEDBL;
	keymap["HASH"]		= K_HASH;
	keymap["DOLLAR"]	= K_DOLLAR;
	keymap["AMPERSAND"]	= K_AMPERSAND;
	keymap["QUOTE"]		= K_QUOTE;
	keymap["LEFTPAREN"]	= K_LEFTPAREN;
	keymap["RIGHTPAREN"]	= K_RIGHTPAREN;
	keymap["ASTERISK"]	= K_ASTERISK;
	keymap["PLUS"]		= K_PLUS;
	keymap["COMMA"]		= K_COMMA;
	keymap["MINUS"]		= K_MINUS;
	keymap["PERIOD"]	= K_PERIOD;
	keymap["SLASH"]		= K_SLASH;
	keymap["0"]		= K_0;
	keymap["1"]		= K_1;
	keymap["2"]		= K_2;
	keymap["3"]		= K_3;
	keymap["4"]		= K_4;
	keymap["5"]		= K_5;
	keymap["6"]		= K_6;
	keymap["7"]		= K_7;
	keymap["8"]		= K_8;
	keymap["9"]		= K_9;
	keymap["COLON"]		= K_COLON;
	keymap["SEMICOLON"]	= K_SEMICOLON;
	keymap["LESS"]		= K_LESS;
	keymap["EQUALS"]	= K_EQUALS;
	keymap["GREATER"]	= K_GREATER;
	keymap["QUESTION"]	= K_QUESTION;
	keymap["AT"]		= K_AT;

	keymap["LEFTBRACKET"]	= K_LEFTBRACKET;
	keymap["BACKSLASH"]	= K_BACKSLASH;
	keymap["RIGHTBRACKET"]	= K_RIGHTBRACKET;
	keymap["CARET"]		= K_CARET;
	keymap["UNDERSCORE"]	= K_UNDERSCORE;
	keymap["BACKQUOTE"]	= K_BACKQUOTE;
	keymap["a"]		= K_a;
	keymap["b"]		= K_b;
	keymap["c"]		= K_c;
	keymap["d"]		= K_d;
	keymap["e"]		= K_e;
	keymap["f"]		= K_f;
	keymap["g"]		= K_g;
	keymap["h"]		= K_h;
	keymap["i"]		= K_i;
	keymap["j"]		= K_j;
	keymap["k"]		= K_k;
	keymap["l"]		= K_l;
	keymap["m"]		= K_m;
	keymap["n"]		= K_n;
	keymap["o"]		= K_o;
	keymap["p"]		= K_p;
	keymap["q"]		= K_q;
	keymap["r"]		= K_r;
	keymap["s"]		= K_s;
	keymap["t"]		= K_t;
	keymap["u"]		= K_u;
	keymap["v"]		= K_v;
	keymap["w"]		= K_w;
	keymap["x"]		= K_x;
	keymap["y"]		= K_y;
	keymap["z"]		= K_z;
	keymap["DELETE"]	= K_DELETE;

	// Numeric keypad
	keymap["KP0"]		= K_KP0;
	keymap["KP1"]		= K_KP1;
	keymap["KP2"]		= K_KP2;
	keymap["KP3"]		= K_KP3;
	keymap["KP4"]		= K_KP4;
	keymap["KP5"]		= K_KP5;
	keymap["KP6"]		= K_KP6;
	keymap["KP7"]		= K_KP7;
	keymap["KP8"]		= K_KP8;
	keymap["KP9"]		= K_KP9;
	keymap["KP_PERIOD"]	= K_KP_PERIOD;
	keymap["KP_DIVIDE"]	= K_KP_DIVIDE;
	keymap["KP_MULTIPLY"]	= K_KP_MULTIPLY;
	keymap["KP_MINUS"]	= K_KP_MINUS;
	keymap["KP_PLUS"]	= K_KP_PLUS;
	keymap["KP_ENTER"]	= K_KP_ENTER;
	keymap["KP_EQUALS"]	= K_KP_EQUALS;

	// Arrows + Home/End pad
	keymap["UP"]		= K_UP;
	keymap["DOWN"]		= K_DOWN;
	keymap["RIGHT"]		= K_RIGHT;
	keymap["LEFT"]		= K_LEFT;
	keymap["INSERT"]	= K_INSERT;
	keymap["HOME"]		= K_HOME;
	keymap["END"]		= K_END;
	keymap["PAGEUP"]	= K_PAGEUP;
	keymap["PAGEDOWN"]	= K_PAGEDOWN;

	// Function keys
	keymap["F1"]		= K_F1;
	keymap["F2"]		= K_F2;
	keymap["F3"]		= K_F3;
	keymap["F4"]		= K_F4;
	keymap["F5"]		= K_F5;
	keymap["F6"]		= K_F6;
	keymap["F7"]		= K_F7;
	keymap["F8"]		= K_F8;
	keymap["F9"]		= K_F9;
	keymap["F10"]		= K_F10;
	keymap["F11"]		= K_F11;
	keymap["F12"]		= K_F12;
	keymap["F13"]		= K_F13;
	keymap["F14"]		= K_F14;
	keymap["F15"]		= K_F15;

	// Key state modifier keys
	keymap["NUMLOCK"]	= K_NUMLOCK;
	keymap["CAPSLOCK"]	= K_CAPSLOCK;
	keymap["SCROLLOCK"]	= K_SCROLLOCK;
	keymap["RSHIFT"]	= K_RSHIFT;
	keymap["LSHIFT"]	= K_LSHIFT;
	keymap["RCTRL"]		= K_RCTRL;
	keymap["LCTRL"]		= K_LCTRL;
	keymap["RALT"]		= K_RALT;
	keymap["LALT"]		= K_LALT;
	keymap["RMETA"]		= K_RMETA;
	keymap["LMETA"]		= K_LMETA;
	keymap["LSUPER"]	= K_LSUPER;	// Left "Windows" key 
	keymap["RSUPER"]	= K_RSUPER;	// Right "Windows" key
	keymap["MODE"]		= K_MODE;	// "Alt Gr" key
	keymap["COMPOSE"]	= K_COMPOSE;	// Multi-key compose key

	// Miscellaneous function keys
	keymap["HELP"]		= K_HELP;
	keymap["PRINT"]		= K_PRINT;
	keymap["SYSREQ"]	= K_SYSREQ;
	keymap["BREAK"]		= K_BREAK;
	keymap["MENU"]		= K_MENU;
	keymap["POWER"]		= K_POWER;	// Power Macintosh power key
	keymap["EURO"]		= K_EURO;	// Some european keyboards
}

bool Keys::init = false;
std::map<const std::string, Keys::KeyCode, Keys::ltstr> Keys::keymap;


