#include "Keys.hh"
#include "StringOp.hh"
#include "stl.hh"
#include <algorithm>
#include <vector>
#include <utility>

using std::string;

namespace openmsx {

namespace Keys {

static std::vector<std::pair<string_ref, KeyCode>> keys;
using CmpKeys = CmpTupleElement<0, StringOp::caseless>;

static void initialize()
{
	static bool init = false;
	if (init) return;
	init = true;

	keys = {
		{ "BACKSPACE",	K_BACKSPACE },
		{ "TAB",	K_TAB },
		{ "CLEAR",	K_CLEAR },
		{ "RETURN",	K_RETURN },
		{ "PAUSE",	K_PAUSE },
		{ "ESCAPE",	K_ESCAPE },
		{ "SPACE",	K_SPACE },
		{ "EXCLAIM",	K_EXCLAIM },
		{ "QUOTEDBL",	K_QUOTEDBL },
		{ "HASH",	K_HASH },
		{ "DOLLAR",	K_DOLLAR },
		{ "AMPERSAND",	K_AMPERSAND },
		{ "QUOTE",	K_QUOTE },
		{ "LEFTPAREN",	K_LEFTPAREN },
		{ "RIGHTPAREN",	K_RIGHTPAREN },
		{ "ASTERISK",	K_ASTERISK },
		{ "PLUS",	K_PLUS },
		{ "COMMA",	K_COMMA },
		{ "MINUS",	K_MINUS },
		{ "PERIOD",	K_PERIOD },
		{ "SLASH",	K_SLASH },
		{ "0",		K_0 },
		{ "1",		K_1 },
		{ "2",		K_2 },
		{ "3",		K_3 },
		{ "4",		K_4 },
		{ "5",		K_5 },
		{ "6",		K_6 },
		{ "7",		K_7 },
		{ "8",		K_8 },
		{ "9",		K_9 },
		{ "COLON",	K_COLON },
		{ "SEMICOLON",	K_SEMICOLON },
		{ "LESS",	K_LESS },
		{ "EQUALS",	K_EQUALS },
		{ "GREATER",	K_GREATER },
		{ "QUESTION",	K_QUESTION },
		{ "AT",		K_AT },

		{ "LEFTBRACKET",K_LEFTBRACKET },
		{ "BACKSLASH",	K_BACKSLASH },
		{ "RIGHTBRACKET",K_RIGHTBRACKET },
		{ "CARET",	K_CARET },
		{ "UNDERSCORE",	K_UNDERSCORE },
		{ "BACKQUOTE",	K_BACKQUOTE },
		{ "A",		K_A },
		{ "B",		K_B },
		{ "C",		K_C },
		{ "D",		K_D },
		{ "E",		K_E },
		{ "F",		K_F },
		{ "G",		K_G },
		{ "H",		K_H },
		{ "I",		K_I },
		{ "J",		K_J },
		{ "K",		K_K },
		{ "L",		K_L },
		{ "M",		K_M },
		{ "N",		K_N },
		{ "O",		K_O },
		{ "P",		K_P },
		{ "Q",		K_Q },
		{ "R",		K_R },
		{ "S",		K_S },
		{ "T",		K_T },
		{ "U",		K_U },
		{ "V",		K_V },
		{ "W",		K_W },
		{ "X",		K_X },
		{ "Y",		K_Y },
		{ "Z",		K_Z },
		{ "DELETE",	K_DELETE },

/*
		{ "WORLD_0",	K_WORLD_0 },
		{ "WORLD_1",	K_WORLD_1 },
		{ "WORLD_2",	K_WORLD_2 },
		{ "WORLD_3",	K_WORLD_3 },
		{ "WORLD_4",	K_WORLD_4 },
		{ "WORLD_5",	K_WORLD_5 },
		{ "WORLD_6",	K_WORLD_6 },
		{ "WORLD_7",	K_WORLD_7 },
		{ "WORLD_8",	K_WORLD_8 },
		{ "WORLD_9",	K_WORLD_9 },
		{ "WORLD_10",	K_WORLD_10 },
		{ "WORLD_11",	K_WORLD_11 },
		{ "WORLD_12",	K_WORLD_12 },
		{ "WORLD_13",	K_WORLD_13 },
		{ "WORLD_14",	K_WORLD_14 },
		{ "WORLD_15",	K_WORLD_15 },
		{ "WORLD_16",	K_WORLD_16 },
		{ "WORLD_17",	K_WORLD_17 },
		{ "WORLD_18",	K_WORLD_18 },
		{ "WORLD_19",	K_WORLD_19 },
		{ "WORLD_20",	K_WORLD_20 },
		{ "WORLD_21",	K_WORLD_21 },
		{ "WORLD_22",	K_WORLD_22 },
		{ "WORLD_23",	K_WORLD_23 },
		{ "WORLD_24",	K_WORLD_24 },
		{ "WORLD_25",	K_WORLD_25 },
		{ "WORLD_26",	K_WORLD_26 },
		{ "WORLD_27",	K_WORLD_27 },
		{ "WORLD_28",	K_WORLD_28 },
		{ "WORLD_29",	K_WORLD_29 },
		{ "WORLD_30",	K_WORLD_30 },
		{ "WORLD_31",	K_WORLD_31 },
		{ "WORLD_32",	K_WORLD_32 },
		{ "WORLD_33",	K_WORLD_33 },
		{ "WORLD_34",	K_WORLD_34 },
		{ "WORLD_35",	K_WORLD_35 },
		{ "WORLD_36",	K_WORLD_36 },
		{ "WORLD_37",	K_WORLD_37 },
		{ "WORLD_38",	K_WORLD_38 },
		{ "WORLD_39",	K_WORLD_39 },
		{ "WORLD_40",	K_WORLD_40 },
		{ "WORLD_41",	K_WORLD_41 },
		{ "WORLD_42",	K_WORLD_42 },
		{ "WORLD_43",	K_WORLD_43 },
		{ "WORLD_44",	K_WORLD_44 },
		{ "WORLD_45",	K_WORLD_45 },
		{ "WORLD_46",	K_WORLD_46 },
		{ "WORLD_47",	K_WORLD_47 },
		{ "WORLD_48",	K_WORLD_48 },
		{ "WORLD_49",	K_WORLD_49 },
		{ "WORLD_50",	K_WORLD_50 },
		{ "WORLD_51",	K_WORLD_51 },
		{ "WORLD_52",	K_WORLD_52 },
		{ "WORLD_53",	K_WORLD_53 },
		{ "WORLD_54",	K_WORLD_54 },
		{ "WORLD_55",	K_WORLD_55 },
		{ "WORLD_56",	K_WORLD_56 },
		{ "WORLD_57",	K_WORLD_57 },
		{ "WORLD_58",	K_WORLD_58 },
		{ "WORLD_59",	K_WORLD_59 },
		{ "WORLD_60",	K_WORLD_60 },
		{ "WORLD_61",	K_WORLD_61 },
		{ "WORLD_62",	K_WORLD_62 },
		{ "WORLD_63",	K_WORLD_63 },
		{ "WORLD_64",	K_WORLD_64 },
		{ "WORLD_65",	K_WORLD_65 },
		{ "WORLD_66",	K_WORLD_66 },
		{ "WORLD_67",	K_WORLD_67 },
		{ "WORLD_68",	K_WORLD_68 },
		{ "WORLD_69",	K_WORLD_69 },
		{ "WORLD_70",	K_WORLD_70 },
		{ "WORLD_71",	K_WORLD_71 },
		{ "WORLD_72",	K_WORLD_72 },
		{ "WORLD_73",	K_WORLD_73 },
		{ "WORLD_74",	K_WORLD_74 },
		{ "WORLD_75",	K_WORLD_75 },
		{ "WORLD_76",	K_WORLD_76 },
		{ "WORLD_77",	K_WORLD_77 },
		{ "WORLD_78",	K_WORLD_78 },
		{ "WORLD_79",	K_WORLD_79 },
		{ "WORLD_80",	K_WORLD_80 },
		{ "WORLD_81",	K_WORLD_81 },
		{ "WORLD_82",	K_WORLD_82 },
		{ "WORLD_83",	K_WORLD_83 },
		{ "WORLD_84",	K_WORLD_84 },
		{ "WORLD_85",	K_WORLD_85 },
		{ "WORLD_86",	K_WORLD_86 },
		{ "WORLD_87",	K_WORLD_87 },
		{ "WORLD_88",	K_WORLD_88 },
		{ "WORLD_89",	K_WORLD_89 },
		{ "WORLD_90",	K_WORLD_90 },
		{ "WORLD_91",	K_WORLD_91 },
		{ "WORLD_92",	K_WORLD_92 },
		{ "WORLD_93",	K_WORLD_93 },
		{ "WORLD_94",	K_WORLD_94 },
		{ "WORLD_95",	K_WORLD_95 },
*/

		// Numeric keypad
		{ "KP0",	K_KP0 },
		{ "KP1",	K_KP1 },
		{ "KP2",	K_KP2 },
		{ "KP3",	K_KP3 },
		{ "KP4",	K_KP4 },
		{ "KP5",	K_KP5 },
		{ "KP6",	K_KP6 },
		{ "KP7",	K_KP7 },
		{ "KP8",	K_KP8 },
		{ "KP9",	K_KP9 },
		{ "KP_PERIOD",	K_KP_PERIOD },
		{ "KP_DIVIDE",	K_KP_DIVIDE },
		{ "KP_MULTIPLY",K_KP_MULTIPLY },
		{ "KP_MINUS",	K_KP_MINUS },
		{ "KP_PLUS",	K_KP_PLUS },
		{ "KP_ENTER",	K_KP_ENTER },
		{ "KP_EQUALS",	K_KP_EQUALS },

		// Arrows + Home/End pad
		{ "UP",		K_UP },
		{ "DOWN",	K_DOWN },
		{ "RIGHT",	K_RIGHT },
		{ "LEFT",	K_LEFT },
		{ "INSERT",	K_INSERT },
		{ "HOME",	K_HOME },
		{ "END",	K_END },
		{ "PAGEUP",	K_PAGEUP },
		{ "PAGEDOWN",	K_PAGEDOWN },

		// Function keys
		{ "F1",		K_F1 },
		{ "F2",		K_F2 },
		{ "F3",		K_F3 },
		{ "F4",		K_F4 },
		{ "F5",		K_F5 },
		{ "F6",		K_F6 },
		{ "F7",		K_F7 },
		{ "F8",		K_F8 },
		{ "F9",		K_F9 },
		{ "F10",	K_F10 },
		{ "F11",	K_F11 },
		{ "F12",	K_F12 },
		{ "F13",	K_F13 },
		{ "F14",	K_F14 },
		{ "F15",	K_F15 },

		// Key state modifier keys
		{ "NUMLOCK",	K_NUMLOCK },
		{ "CAPSLOCK",	K_CAPSLOCK },
		{ "SCROLLOCK",	K_SCROLLOCK },
		{ "RSHIFT",	K_RSHIFT },
		{ "LSHIFT",	K_LSHIFT },
		{ "RCTRL",	K_RCTRL },
		{ "LCTRL",	K_LCTRL },
		{ "RALT",	K_RALT },
		{ "LALT",	K_LALT },
//		{ "RMETA",	K_RMETA },
//		{ "LMETA",	K_LMETA },
		{ "LSUPER",	K_LSUPER },	// Left "Windows" key
		{ "RSUPER",	K_RSUPER },	// Right "Windows" key
		{ "RMODE",	K_MODE },	// "Alt Gr" key
//		{ "COMPOSE",	K_COMPOSE },	// Multi-key compose key

		// Miscellaneous function keys
		{ "HELP",	K_HELP },
		{ "PRINT",	K_PRINT },
		{ "SYSREQ",	K_SYSREQ },
//		{ "BREAK",	K_BREAK },
		{ "MENU",	K_MENU },
		{ "POWER",	K_POWER },	// Power Macintosh power key
//		{ "EURO",	K_EURO },	// Some european keyboards
		{ "UNDO",	K_UNDO },

		// Japanese keyboard special keys
		{ "ZENKAKU_HENKAKU",	K_ZENKAKU_HENKAKU },
		{ "MUHENKAN",		K_MUHENKAN },
		{ "HENKAN_MODE",	K_HENKAN_MODE },
		{ "HIRAGANA_KATAKANA",	K_HIRAGANA_KATAKANA },

		// Modifiers
		{ "SHIFT",	KM_SHIFT },
		{ "CTRL",	KM_CTRL },
		{ "ALT",	KM_ALT },
		{ "META",	KM_META },
		{ "MODE",	KM_MODE },

		// Direction modifiers
		{ "PRESS",	KD_PRESS },
		{ "RELEASE",	KD_RELEASE },
	};
	sort(begin(keys), end(keys), CmpKeys());
}

KeyCode getCode(string_ref name)
{
	initialize();

	auto result = static_cast<KeyCode>(0);
	string_ref::size_type lastPos = 0;
	while (lastPos != string_ref::npos) {
		auto pos = name.substr(lastPos).find_first_of(",+/");
		auto part = (pos != string_ref::npos)
		          ? name.substr(lastPos, pos)
		          : name.substr(lastPos);
		auto it = lower_bound(begin(keys), end(keys), part, CmpKeys());
		StringOp::casecmp cmp;
		if ((it == end(keys)) || !cmp(it->first, part)) {
			return K_NONE;
		}
		KeyCode partCode = it->second;
		if ((partCode & K_MASK) && (result & K_MASK)) {
			// more than one non-modifier component
			// is not allowed
			return K_NONE;
		}
		result = static_cast<KeyCode>(result | partCode);
		lastPos = (pos != string_ref::npos)
		        ? lastPos + pos + 1
		        : string_ref::npos;
	}
	return result;
}

KeyCode getCode(SDL_Keycode key, Uint16 mod, SDL_Scancode scancode, bool release)
{
	KeyCode result;
	switch (key) {
	case SDLK_BACKSPACE:      result = K_BACKSPACE;         break;
	case SDLK_TAB:            result = K_TAB;               break;
	case SDLK_CLEAR:          result = K_CLEAR;             break;
	case SDLK_RETURN:         result = K_RETURN;            break;
	case SDLK_PAUSE:          result = K_PAUSE;             break;
	case SDLK_ESCAPE:         result = K_ESCAPE;            break;
	case SDLK_SPACE:          result = K_SPACE;             break;
	case SDLK_EXCLAIM:        result = K_EXCLAIM;           break;
	case SDLK_QUOTEDBL:       result = K_QUOTEDBL;          break;
	case SDLK_HASH:           result = K_HASH;              break;
	case SDLK_DOLLAR:         result = K_DOLLAR;            break;
	case SDLK_AMPERSAND:      result = K_AMPERSAND;         break;
	case SDLK_QUOTE:          result = K_QUOTE;             break;
	case SDLK_LEFTPAREN:      result = K_LEFTPAREN;         break;
	case SDLK_RIGHTPAREN:     result = K_RIGHTPAREN;        break;
	case SDLK_ASTERISK:       result = K_ASTERISK;          break;
	case SDLK_PLUS:           result = K_PLUS;              break;
	case SDLK_COMMA:          result = K_COMMA;             break;
	case SDLK_MINUS:          result = K_MINUS;             break;
	case SDLK_PERIOD:         result = K_PERIOD;            break;
	case SDLK_SLASH:          result = K_SLASH;             break;
	case SDLK_0:              result = K_0;                 break;
	case SDLK_1:              result = K_1;                 break;
	case SDLK_2:              result = K_2;                 break;
	case SDLK_3:              result = K_3;                 break;
	case SDLK_4:              result = K_4;                 break;
	case SDLK_5:              result = K_5;                 break;
	case SDLK_6:              result = K_6;                 break;
	case SDLK_7:              result = K_7;                 break;
	case SDLK_8:              result = K_8;                 break;
	case SDLK_9:              result = K_9;                 break;
	case SDLK_COLON:          result = K_COLON;             break;
	case SDLK_SEMICOLON:      result = K_SEMICOLON;         break;
	case SDLK_LESS:           result = K_LESS;              break;
	case SDLK_EQUALS:         result = K_EQUALS;            break;
	case SDLK_GREATER:        result = K_GREATER;           break;
	case SDLK_QUESTION:       result = K_QUESTION;          break;
	case SDLK_AT:             result = K_AT;                break;

	case SDLK_LEFTBRACKET:    result = K_LEFTBRACKET;       break;
	case SDLK_BACKSLASH:      result = K_BACKSLASH;         break;
	case SDLK_RIGHTBRACKET:   result = K_RIGHTBRACKET;      break;
	case SDLK_CARET:          result = K_CARET;             break;
	case SDLK_UNDERSCORE:     result = K_UNDERSCORE;        break;
	case SDLK_BACKQUOTE:      result = K_BACKQUOTE;         break;
	case SDLK_a:              result = K_A;                 break;
	case SDLK_b:              result = K_B;                 break;
	case SDLK_c:              result = K_C;                 break;
	case SDLK_d:              result = K_D;                 break;
	case SDLK_e:              result = K_E;                 break;
	case SDLK_f:              result = K_F;                 break;
	case SDLK_g:              result = K_G;                 break;
	case SDLK_h:              result = K_H;                 break;
	case SDLK_i:              result = K_I;                 break;
	case SDLK_j:              result = K_J;                 break;
	case SDLK_k:              result = K_K;                 break;
	case SDLK_l:              result = K_L;                 break;
	case SDLK_m:              result = K_M;                 break;
	case SDLK_n:              result = K_N;                 break;
	case SDLK_o:              result = K_O;                 break;
	case SDLK_p:              result = K_P;                 break;
	case SDLK_q:              result = K_Q;                 break;
	case SDLK_r:              result = K_R;                 break;
	case SDLK_s:              result = K_S;                 break;
	case SDLK_t:              result = K_T;                 break;
	case SDLK_u:              result = K_U;                 break;
	case SDLK_v:              result = K_V;                 break;
	case SDLK_w:              result = K_W;                 break;
	case SDLK_x:              result = K_X;                 break;
	case SDLK_y:              result = K_Y;                 break;
	case SDLK_z:              result = K_Z;                 break;
	case SDLK_DELETE:         result = K_DELETE;            break;

	// Numeric keypad
	case SDLK_KP_0:           result = K_KP0;               break;
	case SDLK_KP_1:           result = K_KP1;               break;
	case SDLK_KP_2:           result = K_KP2;               break;
	case SDLK_KP_3:           result = K_KP3;               break;
	case SDLK_KP_4:           result = K_KP4;               break;
	case SDLK_KP_5:           result = K_KP5;               break;
	case SDLK_KP_6:           result = K_KP6;               break;
	case SDLK_KP_7:           result = K_KP7;               break;
	case SDLK_KP_8:           result = K_KP8;               break;
	case SDLK_KP_9:           result = K_KP9;               break;
	case SDLK_KP_PERIOD:      result = K_KP_PERIOD;         break;
	case SDLK_KP_DIVIDE:      result = K_KP_DIVIDE;         break;
	case SDLK_KP_MULTIPLY:    result = K_KP_MULTIPLY;       break;
	case SDLK_KP_MINUS:       result = K_KP_MINUS;          break;
	case SDLK_KP_PLUS:        result = K_KP_PLUS;           break;
	case SDLK_KP_ENTER:       result = K_KP_ENTER;          break;
	case SDLK_KP_EQUALS:      result = K_KP_EQUALS;         break;

	// Arrows + Home/End pad
	case SDLK_UP:             result = K_UP;                break;
	case SDLK_DOWN:           result = K_DOWN;              break;
	case SDLK_RIGHT:          result = K_RIGHT;             break;
	case SDLK_LEFT:           result = K_LEFT;              break;
	case SDLK_INSERT:         result = K_INSERT;            break;
	case SDLK_HOME:           result = K_HOME;              break;
	case SDLK_END:            result = K_END;               break;
	case SDLK_PAGEUP:         result = K_PAGEUP;            break;
	case SDLK_PAGEDOWN:       result = K_PAGEDOWN;          break;

	// Function keys
	case SDLK_F1:             result = K_F1;                break;
	case SDLK_F2:             result = K_F2;                break;
	case SDLK_F3:             result = K_F3;                break;
	case SDLK_F4:             result = K_F4;                break;
	case SDLK_F5:             result = K_F5;                break;
	case SDLK_F6:             result = K_F6;                break;
	case SDLK_F7:             result = K_F7;                break;
	case SDLK_F8:             result = K_F8;                break;
	case SDLK_F9:             result = K_F9;                break;
	case SDLK_F10:            result = K_F10;               break;
	case SDLK_F11:            result = K_F11;               break;
	case SDLK_F12:            result = K_F12;               break;
	case SDLK_F13:            result = K_F13;               break;
	case SDLK_F14:            result = K_F14;               break;
	case SDLK_F15:            result = K_F15;               break;

	// Key state modifier keys
	case SDLK_NUMLOCKCLEAR:   result = K_NUMLOCK;           break;
	case SDLK_CAPSLOCK:       result = K_CAPSLOCK;          break;
	case SDLK_SCROLLLOCK:     result = K_SCROLLOCK;         break;
	case SDLK_RSHIFT:         result = K_RSHIFT;            break;
	case SDLK_LSHIFT:         result = K_LSHIFT;            break;
	case SDLK_RCTRL:          result = K_RCTRL;             break;
	case SDLK_LCTRL:          result = K_LCTRL;             break;
	case SDLK_RALT:           result = K_RALT;              break;
	case SDLK_LALT:           result = K_LALT;              break;
//	case SDLK_RMETA:          result = K_RMETA;             break;
//	case SDLK_LMETA:          result = K_LMETA;             break;
	case SDLK_LGUI:           result = K_LSUPER;            break; // Left "Windows" key
	case SDLK_RGUI:           result = K_RSUPER;            break; // Right "Windows" key
	case SDLK_MODE:           result = K_MODE;              break; // "Alt Gr" key
//	case SDLK_COMPOSE:        result = K_COMPOSE;           break; // Multi-key compose key

	// Miscellaneous function keys
	case SDLK_HELP:           result = K_HELP;              break;
	case SDLK_PRINTSCREEN:    result = K_PRINT;             break;
	case SDLK_SYSREQ:         result = K_SYSREQ;            break;
//	case SDLK_BREAK:          result = K_BREAK;             break;
	case SDLK_MENU:           result = K_MENU;              break;
	case SDLK_POWER:          result = K_POWER;             break; // Power Macintosh power key
//	case SDLK_EURO:           result = K_EURO;              break; // Some european keyboards
	case SDLK_UNDO:           result = K_UNDO;              break;

	default:                  result = K_UNKNOWN;           break;
	}

	// Handle keys that don't have a key code but do have a scan code.
	if (result == K_UNKNOWN) {
		// Assume it is a Japanese keyboard and check
		// scancode to recognize a few japanese
		// specific keys for which SDL does not have an
		// SDL_Keycode keysym definition.
		switch (scancode) {
		// Keys found on Japanese keyboards:
		case 49:
			result = K_ZENKAKU_HENKAKU;
			break;
		case 129:
			result = K_HENKAN_MODE;
			break;
		case 131:
			result = K_MUHENKAN;
			break;
		case 208:
			result = K_HIRAGANA_KATAKANA;
			break;
		// Keys found on Korean keyboards:
		case 56:
			// On Korean keyboard this code is used for R-ALT key but SDL does
			// not seem to recognize it, as reported by Miso Kim.
			result = K_RALT;
		default:
			break; // nothing, silence compiler warning
		}
	}

	// Apply modifiers.
	if (mod & KMOD_CTRL) {
		result = static_cast<KeyCode>(result | KM_CTRL);
	}
	if (mod & KMOD_SHIFT) {
		result = static_cast<KeyCode>(result | KM_SHIFT);
	}
	if (mod & KMOD_ALT) {
		result = static_cast<KeyCode>(result | KM_ALT);
	}
	if (mod & KMOD_GUI) {
		result = static_cast<KeyCode>(result | KM_META);
	}
	if (mod & KMOD_MODE) {
		result = static_cast<KeyCode>(result | KM_MODE);
	}

	if (release) {
		result = static_cast<KeyCode>(result | KD_RELEASE);
	}
	return result;
}

const string getName(KeyCode keyCode)
{
	initialize();

	string result;
	for (auto& p : keys) {
		if (p.second == (keyCode & K_MASK)) {
			result = p.first.str();
			break;
		}
	}
	if (result.empty()) {
		return "unknown";
	}
	if (keyCode & KM_CTRL) {
		result += "+CTRL";
	}
	if (keyCode & KM_SHIFT) {
		result += "+SHIFT";
	}
	if (keyCode & KM_ALT) {
		result += "+ALT";
	}
	if (keyCode & KM_META) {
		result += "+META";
	}
	if (keyCode & KM_MODE) {
		result += "+MODE";
	}
	if (keyCode & KD_RELEASE) {
		result += ",RELEASE";
	}
	return result;
}

} // namespace Keys

} // namespace openmsx
