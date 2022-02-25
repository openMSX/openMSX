#include "Keys.hh"
#include "StringOp.hh"
#include "cstd.hh"
#include "ranges.hh"
#include <array>

using std::string_view;

namespace openmsx::Keys {

// can be std::pair in C++20
struct P {
	constexpr P(string_view s, KeyCode k)
		: first(s), second(k) {}

	string_view first;
	KeyCode second;
};

struct CmpKeys {
	// for cstd::sort
	[[nodiscard]] constexpr bool operator()(const P& x, const P& y) const {
		return x.first < y.first; // shortcut: no need to ignore case
	}

	// for lower_bound()
	[[nodiscard]] bool operator()(const P& x, string_view y) const {
		StringOp::caseless cmp;
		return cmp(x.first, y);
	}
};

#ifdef _MSC_VER
// Require a new enough visual studio version:
//   https://developercommunity.visualstudio.com/content/problem/429980/wrong-code-with-constexpr-on-msvc-1916.html
static_assert(_MSC_VER >= 1923, "visual studio version too old");
#endif
static constexpr auto getSortedKeys()
{
	auto keys = std::array{
		P("BACKSPACE",	K_BACKSPACE),
		P("TAB",	K_TAB),
		P("CLEAR",	K_CLEAR),
		P("RETURN",	K_RETURN),
		P("PAUSE",	K_PAUSE),
		P("ESCAPE",	K_ESCAPE),
		P("SPACE",	K_SPACE),
		P("EXCLAIM",	K_EXCLAIM),
		P("QUOTEDBL",	K_QUOTEDBL),
		P("HASH",	K_HASH),
		P("DOLLAR",	K_DOLLAR),
		P("AMPERSAND",	K_AMPERSAND),
		P("QUOTE",	K_QUOTE),
		P("LEFTPAREN",	K_LEFTPAREN),
		P("RIGHTPAREN",	K_RIGHTPAREN),
		P("ASTERISK",	K_ASTERISK),
		P("PLUS",	K_PLUS),
		P("COMMA",	K_COMMA),
		P("MINUS",	K_MINUS),
		P("PERIOD",	K_PERIOD),
		P("SLASH",	K_SLASH),
		P("0",		K_0),
		P("1",		K_1),
		P("2",		K_2),
		P("3",		K_3),
		P("4",		K_4),
		P("5",		K_5),
		P("6",		K_6),
		P("7",		K_7),
		P("8",		K_8),
		P("9",		K_9),
		P("COLON",	K_COLON),
		P("SEMICOLON",	K_SEMICOLON),
		P("LESS",	K_LESS),
		P("EQUALS",	K_EQUALS),
		P("GREATER",	K_GREATER),
		P("QUESTION",	K_QUESTION),
		P("AT",		K_AT),

		P("LEFTBRACKET",K_LEFTBRACKET),
		P("BACKSLASH",	K_BACKSLASH),
		P("RIGHTBRACKET",K_RIGHTBRACKET),
		P("CARET",	K_CARET),
		P("UNDERSCORE",	K_UNDERSCORE),
		P("BACKQUOTE",	K_BACKQUOTE),
		P("A",		K_A),
		P("B",		K_B),
		P("C",		K_C),
		P("D",		K_D),
		P("E",		K_E),
		P("F",		K_F),
		P("G",		K_G),
		P("H",		K_H),
		P("I",		K_I),
		P("J",		K_J),
		P("K",		K_K),
		P("L",		K_L),
		P("M",		K_M),
		P("N",		K_N),
		P("O",		K_O),
		P("P",		K_P),
		P("Q",		K_Q),
		P("R",		K_R),
		P("S",		K_S),
		P("T",		K_T),
		P("U",		K_U),
		P("V",		K_V),
		P("W",		K_W),
		P("X",		K_X),
		P("Y",		K_Y),
		P("Z",		K_Z),
		P("DELETE",	K_DELETE),

/*
		P("WORLD_0",	K_WORLD_0),
		P("WORLD_1",	K_WORLD_1),
		P("WORLD_2",	K_WORLD_2),
		P("WORLD_3",	K_WORLD_3),
		P("WORLD_4",	K_WORLD_4),
		P("WORLD_5",	K_WORLD_5),
		P("WORLD_6",	K_WORLD_6),
		P("WORLD_7",	K_WORLD_7),
		P("WORLD_8",	K_WORLD_8),
		P("WORLD_9",	K_WORLD_9),
		P("WORLD_10",	K_WORLD_10),
		P("WORLD_11",	K_WORLD_11),
		P("WORLD_12",	K_WORLD_12),
		P("WORLD_13",	K_WORLD_13),
		P("WORLD_14",	K_WORLD_14),
		P("WORLD_15",	K_WORLD_15),
		P("WORLD_16",	K_WORLD_16),
		P("WORLD_17",	K_WORLD_17),
		P("WORLD_18",	K_WORLD_18),
		P("WORLD_19",	K_WORLD_19),
		P("WORLD_20",	K_WORLD_20),
		P("WORLD_21",	K_WORLD_21),
		P("WORLD_22",	K_WORLD_22),
		P("WORLD_23",	K_WORLD_23),
		P("WORLD_24",	K_WORLD_24),
		P("WORLD_25",	K_WORLD_25),
		P("WORLD_26",	K_WORLD_26),
		P("WORLD_27",	K_WORLD_27),
		P("WORLD_28",	K_WORLD_28),
		P("WORLD_29",	K_WORLD_29),
		P("WORLD_30",	K_WORLD_30),
		P("WORLD_31",	K_WORLD_31),
		P("WORLD_32",	K_WORLD_32),
		P("WORLD_33",	K_WORLD_33),
		P("WORLD_34",	K_WORLD_34),
		P("WORLD_35",	K_WORLD_35),
		P("WORLD_36",	K_WORLD_36),
		P("WORLD_37",	K_WORLD_37),
		P("WORLD_38",	K_WORLD_38),
		P("WORLD_39",	K_WORLD_39),
		P("WORLD_40",	K_WORLD_40),
		P("WORLD_41",	K_WORLD_41),
		P("WORLD_42",	K_WORLD_42),
		P("WORLD_43",	K_WORLD_43),
		P("WORLD_44",	K_WORLD_44),
		P("WORLD_45",	K_WORLD_45),
		P("WORLD_46",	K_WORLD_46),
		P("WORLD_47",	K_WORLD_47),
		P("WORLD_48",	K_WORLD_48),
		P("WORLD_49",	K_WORLD_49),
		P("WORLD_50",	K_WORLD_50),
		P("WORLD_51",	K_WORLD_51),
		P("WORLD_52",	K_WORLD_52),
		P("WORLD_53",	K_WORLD_53),
		P("WORLD_54",	K_WORLD_54),
		P("WORLD_55",	K_WORLD_55),
		P("WORLD_56",	K_WORLD_56),
		P("WORLD_57",	K_WORLD_57),
		P("WORLD_58",	K_WORLD_58),
		P("WORLD_59",	K_WORLD_59),
		P("WORLD_60",	K_WORLD_60),
		P("WORLD_61",	K_WORLD_61),
		P("WORLD_62",	K_WORLD_62),
		P("WORLD_63",	K_WORLD_63),
		P("WORLD_64",	K_WORLD_64),
		P("WORLD_65",	K_WORLD_65),
		P("WORLD_66",	K_WORLD_66),
		P("WORLD_67",	K_WORLD_67),
		P("WORLD_68",	K_WORLD_68),
		P("WORLD_69",	K_WORLD_69),
		P("WORLD_70",	K_WORLD_70),
		P("WORLD_71",	K_WORLD_71),
		P("WORLD_72",	K_WORLD_72),
		P("WORLD_73",	K_WORLD_73),
		P("WORLD_74",	K_WORLD_74),
		P("WORLD_75",	K_WORLD_75),
		P("WORLD_76",	K_WORLD_76),
		P("WORLD_77",	K_WORLD_77),
		P("WORLD_78",	K_WORLD_78),
		P("WORLD_79",	K_WORLD_79),
		P("WORLD_80",	K_WORLD_80),
		P("WORLD_81",	K_WORLD_81),
		P("WORLD_82",	K_WORLD_82),
		P("WORLD_83",	K_WORLD_83),
		P("WORLD_84",	K_WORLD_84),
		P("WORLD_85",	K_WORLD_85),
		P("WORLD_86",	K_WORLD_86),
		P("WORLD_87",	K_WORLD_87),
		P("WORLD_88",	K_WORLD_88),
		P("WORLD_89",	K_WORLD_89),
		P("WORLD_90",	K_WORLD_90),
		P("WORLD_91",	K_WORLD_91),
		P("WORLD_92",	K_WORLD_92),
		P("WORLD_93",	K_WORLD_93),
		P("WORLD_94",	K_WORLD_94),
		P("WORLD_95",	K_WORLD_95),
*/

		// Numeric keypad
		P("KP0",	K_KP0),
		P("KP1",	K_KP1),
		P("KP2",	K_KP2),
		P("KP3",	K_KP3),
		P("KP4",	K_KP4),
		P("KP5",	K_KP5),
		P("KP6",	K_KP6),
		P("KP7",	K_KP7),
		P("KP8",	K_KP8),
		P("KP9",	K_KP9),
		P("KP_PERIOD",	K_KP_PERIOD),
		P("KP_DIVIDE",	K_KP_DIVIDE),
		P("KP_MULTIPLY",K_KP_MULTIPLY),
		P("KP_MINUS",	K_KP_MINUS),
		P("KP_PLUS",	K_KP_PLUS),
		P("KP_ENTER",	K_KP_ENTER),
		P("KP_EQUALS",	K_KP_EQUALS),

		// Arrows + Home/End pad
		P("UP",		K_UP),
		P("DOWN",	K_DOWN),
		P("RIGHT",	K_RIGHT),
		P("LEFT",	K_LEFT),
		P("INSERT",	K_INSERT),
		P("HOME",	K_HOME),
		P("END",	K_END),
		P("PAGEUP",	K_PAGEUP),
		P("PAGEDOWN",	K_PAGEDOWN),

		// Function keys
		P("F1",		K_F1),
		P("F2",		K_F2),
		P("F3",		K_F3),
		P("F4",		K_F4),
		P("F5",		K_F5),
		P("F6",		K_F6),
		P("F7",		K_F7),
		P("F8",		K_F8),
		P("F9",		K_F9),
		P("F10",	K_F10),
		P("F11",	K_F11),
		P("F12",	K_F12),
		P("F13",	K_F13),
		P("F14",	K_F14),
		P("F15",	K_F15),
		P("F16",	K_F16),
		P("F17",	K_F17),
		P("F18",	K_F18),
		P("F19",	K_F19),
		P("F20",	K_F20),
		P("F21",	K_F21),
		P("F22",	K_F22),
		P("F23",	K_F23),
		P("F24",	K_F24),

		// Key state modifier keys
		P("NUMLOCK",	K_NUMLOCK),
		P("CAPSLOCK",	K_CAPSLOCK),
		// Note: Older openMSX releases duplicated the SDL 1.2 spelling mistake
		//       in the scroll lock key symbol. Since the wrong spelling comes
		//       alphabetically after the right one, our to-string conversion
		//       will pick the right one.
		P("SCROLLLOCK",	K_SCROLLLOCK),
		P("SCROLLOCK",	K_SCROLLLOCK), // backwards compat
		P("RSHIFT",	K_RSHIFT),
		P("LSHIFT",	K_LSHIFT),
		P("RCTRL",	K_RCTRL),
		P("LCTRL",	K_LCTRL),
		P("RALT",	K_RALT),
		P("LALT",	K_LALT),
//		P("RMETA",	K_RMETA),
//		P("LMETA",	K_LMETA),
		P("LSUPER",	K_LSUPER),	// Left "Windows" key
		P("RSUPER",	K_RSUPER),	// Right "Windows" key
		P("RMODE",	K_MODE),	// "Alt Gr" key
//		P("COMPOSE",	K_COMPOSE),	// Multi-key compose key

		// Miscellaneous function keys
		P("HELP",	K_HELP),
		P("PRINT",	K_PRINT),
		P("SYSREQ",	K_SYSREQ),
//		P("BREAK",	K_BREAK),
		P("MENU",	K_MENU),
		P("POWER",	K_POWER),	// Power Macintosh power key
//		P("EURO",	K_EURO),	// Some european keyboards
		P("UNDO",	K_UNDO),

		// Application Control keys
		P("BACK",	K_BACK),

		// Japanese keyboard special keys
		P("ZENKAKU_HENKAKU",	K_ZENKAKU_HENKAKU),
		P("MUHENKAN",		K_MUHENKAN),
		P("HENKAN_MODE",	K_HENKAN_MODE),
		P("HIRAGANA_KATAKANA",	K_HIRAGANA_KATAKANA),

		// Modifiers
		P("SHIFT",	KM_SHIFT),
		P("CTRL",	KM_CTRL),
		P("ALT",	KM_ALT),
		P("META",	KM_META),
		P("MODE",	KM_MODE),

		// Direction modifiers
		P("PRESS",	KD_PRESS),
		P("RELEASE",	KD_RELEASE)
	};
	cstd::sort(keys, CmpKeys());
	return keys;
}

static constexpr auto keys = getSortedKeys();

KeyCode getCode(string_view name)
{
	auto result = static_cast<KeyCode>(0);
	string_view::size_type lastPos = 0;
	while (lastPos != string_view::npos) {
		auto pos = name.substr(lastPos).find_first_of(",+/");
		auto part = (pos != string_view::npos)
		          ? name.substr(lastPos, pos)
		          : name.substr(lastPos);
		auto it = ranges::lower_bound(keys, part, CmpKeys());
		StringOp::casecmp cmp;
		if ((it == std::end(keys)) || !cmp(it->first, part)) {
			return K_NONE;
		}
		KeyCode partCode = it->second;
		if ((partCode & K_MASK) && (result & K_MASK)) {
			// more than one non-modifier component
			// is not allowed
			return K_NONE;
		}
		result = static_cast<KeyCode>(result | partCode);
		lastPos = (pos != string_view::npos)
		        ? lastPos + pos + 1
		        : string_view::npos;
	}
	return result;
}

std::pair<KeyCode, KeyCode> getCodes(SDL_Keycode sdlkeycode, Uint16 mod, SDL_Scancode sdlscancode, bool release)
{
	KeyCode key = [&] {
		switch (sdlkeycode) {
		case SDLK_BACKSPACE:      return K_BACKSPACE;
		case SDLK_TAB:            return K_TAB;
		case SDLK_CLEAR:          return K_CLEAR;
		case SDLK_RETURN:         return K_RETURN;
		case SDLK_PAUSE:          return K_PAUSE;
		case SDLK_ESCAPE:         return K_ESCAPE;
		case SDLK_SPACE:          return K_SPACE;
		case SDLK_EXCLAIM:        return K_EXCLAIM;
		case SDLK_QUOTEDBL:       return K_QUOTEDBL;
		case SDLK_HASH:           return K_HASH;
		case SDLK_DOLLAR:         return K_DOLLAR;
		case SDLK_AMPERSAND:      return K_AMPERSAND;
		case SDLK_QUOTE:          return K_QUOTE;
		case SDLK_LEFTPAREN:      return K_LEFTPAREN;
		case SDLK_RIGHTPAREN:     return K_RIGHTPAREN;
		case SDLK_ASTERISK:       return K_ASTERISK;
		case SDLK_PLUS:           return K_PLUS;
		case SDLK_COMMA:          return K_COMMA;
		case SDLK_MINUS:          return K_MINUS;
		case SDLK_PERIOD:         return K_PERIOD;
		case SDLK_SLASH:          return K_SLASH;
		case SDLK_0:              return K_0;
		case SDLK_1:              return K_1;
		case SDLK_2:              return K_2;
		case SDLK_3:              return K_3;
		case SDLK_4:              return K_4;
		case SDLK_5:              return K_5;
		case SDLK_6:              return K_6;
		case SDLK_7:              return K_7;
		case SDLK_8:              return K_8;
		case SDLK_9:              return K_9;
		case SDLK_COLON:          return K_COLON;
		case SDLK_SEMICOLON:      return K_SEMICOLON;
		case SDLK_LESS:           return K_LESS;
		case SDLK_EQUALS:         return K_EQUALS;
		case SDLK_GREATER:        return K_GREATER;
		case SDLK_QUESTION:       return K_QUESTION;
		case SDLK_AT:             return K_AT;

		case SDLK_LEFTBRACKET:    return K_LEFTBRACKET;
		case SDLK_BACKSLASH:      return K_BACKSLASH;
		case SDLK_RIGHTBRACKET:   return K_RIGHTBRACKET;
		case SDLK_CARET:          return K_CARET;
		case SDLK_UNDERSCORE:     return K_UNDERSCORE;
		case SDLK_BACKQUOTE:      return K_BACKQUOTE;
		case SDLK_a:              return K_A;
		case SDLK_b:              return K_B;
		case SDLK_c:              return K_C;
		case SDLK_d:              return K_D;
		case SDLK_e:              return K_E;
		case SDLK_f:              return K_F;
		case SDLK_g:              return K_G;
		case SDLK_h:              return K_H;
		case SDLK_i:              return K_I;
		case SDLK_j:              return K_J;
		case SDLK_k:              return K_K;
		case SDLK_l:              return K_L;
		case SDLK_m:              return K_M;
		case SDLK_n:              return K_N;
		case SDLK_o:              return K_O;
		case SDLK_p:              return K_P;
		case SDLK_q:              return K_Q;
		case SDLK_r:              return K_R;
		case SDLK_s:              return K_S;
		case SDLK_t:              return K_T;
		case SDLK_u:              return K_U;
		case SDLK_v:              return K_V;
		case SDLK_w:              return K_W;
		case SDLK_x:              return K_X;
		case SDLK_y:              return K_Y;
		case SDLK_z:              return K_Z;
		case SDLK_DELETE:         return K_DELETE;

		// Numeric keypad
		case SDLK_KP_0:           return K_KP0;
		case SDLK_KP_1:           return K_KP1;
		case SDLK_KP_2:           return K_KP2;
		case SDLK_KP_3:           return K_KP3;
		case SDLK_KP_4:           return K_KP4;
		case SDLK_KP_5:           return K_KP5;
		case SDLK_KP_6:           return K_KP6;
		case SDLK_KP_7:           return K_KP7;
		case SDLK_KP_8:           return K_KP8;
		case SDLK_KP_9:           return K_KP9;
		case SDLK_KP_PERIOD:      return K_KP_PERIOD;
		case SDLK_KP_DIVIDE:      return K_KP_DIVIDE;
		case SDLK_KP_MULTIPLY:    return K_KP_MULTIPLY;
		case SDLK_KP_MINUS:       return K_KP_MINUS;
		case SDLK_KP_PLUS:        return K_KP_PLUS;
		case SDLK_KP_ENTER:       return K_KP_ENTER;
		case SDLK_KP_EQUALS:      return K_KP_EQUALS;

		// Arrows + Home/End pad
		case SDLK_UP:             return K_UP;
		case SDLK_DOWN:           return K_DOWN;
		case SDLK_RIGHT:          return K_RIGHT;
		case SDLK_LEFT:           return K_LEFT;
		case SDLK_INSERT:         return K_INSERT;
		case SDLK_HOME:           return K_HOME;
		case SDLK_END:            return K_END;
		case SDLK_PAGEUP:         return K_PAGEUP;
		case SDLK_PAGEDOWN:       return K_PAGEDOWN;

		// Function keys
		case SDLK_F1:             return K_F1;
		case SDLK_F2:             return K_F2;
		case SDLK_F3:             return K_F3;
		case SDLK_F4:             return K_F4;
		case SDLK_F5:             return K_F5;
		case SDLK_F6:             return K_F6;
		case SDLK_F7:             return K_F7;
		case SDLK_F8:             return K_F8;
		case SDLK_F9:             return K_F9;
		case SDLK_F10:            return K_F10;
		case SDLK_F11:            return K_F11;
		case SDLK_F12:            return K_F12;
		case SDLK_F13:            return K_F13;
		case SDLK_F14:            return K_F14;
		case SDLK_F15:            return K_F15;
		case SDLK_F16:            return K_F16;
		case SDLK_F17:            return K_F17;
		case SDLK_F18:            return K_F18;
		case SDLK_F19:            return K_F19;
		case SDLK_F20:            return K_F20;
		case SDLK_F21:            return K_F21;
		case SDLK_F22:            return K_F22;
		case SDLK_F23:            return K_F23;
		case SDLK_F24:            return K_F24;

		// Key state modifier keys
		case SDLK_NUMLOCKCLEAR:   return K_NUMLOCK;
		case SDLK_CAPSLOCK:       return K_CAPSLOCK;
		case SDLK_SCROLLLOCK:     return K_SCROLLLOCK;
		case SDLK_RSHIFT:         return K_RSHIFT;
		case SDLK_LSHIFT:         return K_LSHIFT;
		case SDLK_RCTRL:          return K_RCTRL;
		case SDLK_LCTRL:          return K_LCTRL;
		case SDLK_RALT:           return K_RALT;
		case SDLK_LALT:           return K_LALT;
	//	case SDLK_RMETA:          return K_RMETA;
	//	case SDLK_LMETA:          return K_LMETA;
		case SDLK_LGUI:           return K_LSUPER;      // Left "Windows" key
		case SDLK_RGUI:           return K_RSUPER;      // Right "Windows" key
		case SDLK_MODE:           return K_MODE;        // "Alt Gr" key
	//	case SDLK_COMPOSE:        return K_COMPOSE;     // Multi-key compose key

		// Miscellaneous function keys
		case SDLK_HELP:           return K_HELP;
		case SDLK_PRINTSCREEN:    return K_PRINT;
		case SDLK_SYSREQ:         return K_SYSREQ;
	//	case SDLK_BREAK:          return K_BREAK;
		case SDLK_APPLICATION:    return K_MENU;
		case SDLK_MENU:           return K_MENU;
		case SDLK_POWER:          return K_POWER;       // Power Macintosh power key
	//	case SDLK_EURO:           return K_EURO;        // Some european keyboards
		case SDLK_UNDO:           return K_UNDO;

		// Application Control keys
		case SDLK_AC_BACK:        return K_BACK;

		default:                  return K_UNKNOWN;
		}
	}();

	KeyCode scan = [&] {
		switch (sdlscancode) {
		case SDL_SCANCODE_BACKSPACE:      return K_BACKSPACE;
		case SDL_SCANCODE_TAB:            return K_TAB;
		case SDL_SCANCODE_CLEAR:          return K_CLEAR;
		case SDL_SCANCODE_RETURN:         return K_RETURN;
		case SDL_SCANCODE_PAUSE:          return K_PAUSE;
		case SDL_SCANCODE_ESCAPE:         return K_ESCAPE;
		case SDL_SCANCODE_SPACE:          return K_SPACE;
		case SDL_SCANCODE_APOSTROPHE:     return K_QUOTE;
		case SDL_SCANCODE_COMMA:          return K_COMMA;
		case SDL_SCANCODE_MINUS:          return K_MINUS;
		case SDL_SCANCODE_PERIOD:         return K_PERIOD;
		case SDL_SCANCODE_SLASH:          return K_SLASH;
		case SDL_SCANCODE_0:              return K_0;
		case SDL_SCANCODE_1:              return K_1;
		case SDL_SCANCODE_2:              return K_2;
		case SDL_SCANCODE_3:              return K_3;
		case SDL_SCANCODE_4:              return K_4;
		case SDL_SCANCODE_5:              return K_5;
		case SDL_SCANCODE_6:              return K_6;
		case SDL_SCANCODE_7:              return K_7;
		case SDL_SCANCODE_8:              return K_8;
		case SDL_SCANCODE_9:              return K_9;
		case SDL_SCANCODE_SEMICOLON:      return K_SEMICOLON;
		case SDL_SCANCODE_EQUALS:         return K_EQUALS;

		case SDL_SCANCODE_LEFTBRACKET:    return K_LEFTBRACKET;
		case SDL_SCANCODE_BACKSLASH:      return K_BACKSLASH;
		case SDL_SCANCODE_RIGHTBRACKET:   return K_RIGHTBRACKET;
		case SDL_SCANCODE_GRAVE:          return K_BACKQUOTE;
		case SDL_SCANCODE_A:              return K_A;
		case SDL_SCANCODE_B:              return K_B;
		case SDL_SCANCODE_C:              return K_C;
		case SDL_SCANCODE_D:              return K_D;
		case SDL_SCANCODE_E:              return K_E;
		case SDL_SCANCODE_F:              return K_F;
		case SDL_SCANCODE_G:              return K_G;
		case SDL_SCANCODE_H:              return K_H;
		case SDL_SCANCODE_I:              return K_I;
		case SDL_SCANCODE_J:              return K_J;
		case SDL_SCANCODE_K:              return K_K;
		case SDL_SCANCODE_L:              return K_L;
		case SDL_SCANCODE_M:              return K_M;
		case SDL_SCANCODE_N:              return K_N;
		case SDL_SCANCODE_O:              return K_O;
		case SDL_SCANCODE_P:              return K_P;
		case SDL_SCANCODE_Q:              return K_Q;
		case SDL_SCANCODE_R:              return K_R;
		case SDL_SCANCODE_S:              return K_S;
		case SDL_SCANCODE_T:              return K_T;
		case SDL_SCANCODE_U:              return K_U;
		case SDL_SCANCODE_V:              return K_V;
		case SDL_SCANCODE_W:              return K_W;
		case SDL_SCANCODE_X:              return K_X;
		case SDL_SCANCODE_Y:              return K_Y;
		case SDL_SCANCODE_Z:              return K_Z;
		case SDL_SCANCODE_DELETE:         return K_DELETE;

		// Numeric keypad
		case SDL_SCANCODE_KP_0:           return K_KP0;
		case SDL_SCANCODE_KP_1:           return K_KP1;
		case SDL_SCANCODE_KP_2:           return K_KP2;
		case SDL_SCANCODE_KP_3:           return K_KP3;
		case SDL_SCANCODE_KP_4:           return K_KP4;
		case SDL_SCANCODE_KP_5:           return K_KP5;
		case SDL_SCANCODE_KP_6:           return K_KP6;
		case SDL_SCANCODE_KP_7:           return K_KP7;
		case SDL_SCANCODE_KP_8:           return K_KP8;
		case SDL_SCANCODE_KP_9:           return K_KP9;
		case SDL_SCANCODE_KP_PERIOD:      return K_KP_PERIOD;
		case SDL_SCANCODE_KP_DIVIDE:      return K_KP_DIVIDE;
		case SDL_SCANCODE_KP_MULTIPLY:    return K_KP_MULTIPLY;
		case SDL_SCANCODE_KP_MINUS:       return K_KP_MINUS;
		case SDL_SCANCODE_KP_PLUS:        return K_KP_PLUS;
		case SDL_SCANCODE_KP_ENTER:       return K_KP_ENTER;
		case SDL_SCANCODE_KP_EQUALS:      return K_KP_EQUALS;

		// Arrows + Home/End pad
		case SDL_SCANCODE_UP:             return K_UP;
		case SDL_SCANCODE_DOWN:           return K_DOWN;
		case SDL_SCANCODE_RIGHT:          return K_RIGHT;
		case SDL_SCANCODE_LEFT:           return K_LEFT;
		case SDL_SCANCODE_INSERT:         return K_INSERT;
		case SDL_SCANCODE_HOME:           return K_HOME;
		case SDL_SCANCODE_END:            return K_END;
		case SDL_SCANCODE_PAGEUP:         return K_PAGEUP;
		case SDL_SCANCODE_PAGEDOWN:       return K_PAGEDOWN;

		// Function keys
		case SDL_SCANCODE_F1:             return K_F1;
		case SDL_SCANCODE_F2:             return K_F2;
		case SDL_SCANCODE_F3:             return K_F3;
		case SDL_SCANCODE_F4:             return K_F4;
		case SDL_SCANCODE_F5:             return K_F5;
		case SDL_SCANCODE_F6:             return K_F6;
		case SDL_SCANCODE_F7:             return K_F7;
		case SDL_SCANCODE_F8:             return K_F8;
		case SDL_SCANCODE_F9:             return K_F9;
		case SDL_SCANCODE_F10:            return K_F10;
		case SDL_SCANCODE_F11:            return K_F11;
		case SDL_SCANCODE_F12:            return K_F12;
		case SDL_SCANCODE_F13:            return K_F13;
		case SDL_SCANCODE_F14:            return K_F14;
		case SDL_SCANCODE_F15:            return K_F15;
		case SDL_SCANCODE_F16:            return K_F16;
		case SDL_SCANCODE_F17:            return K_F17;
		case SDL_SCANCODE_F18:            return K_F18;
		case SDL_SCANCODE_F19:            return K_F19;
		case SDL_SCANCODE_F20:            return K_F20;
		case SDL_SCANCODE_F21:            return K_F21;
		case SDL_SCANCODE_F22:            return K_F22;
		case SDL_SCANCODE_F23:            return K_F23;
		case SDL_SCANCODE_F24:            return K_F24;

		// Key state modifier keys
		case SDL_SCANCODE_NUMLOCKCLEAR:   return K_NUMLOCK;
		case SDL_SCANCODE_CAPSLOCK:       return K_CAPSLOCK;
		case SDL_SCANCODE_SCROLLLOCK:     return K_SCROLLLOCK;
		case SDL_SCANCODE_RSHIFT:         return K_RSHIFT;
		case SDL_SCANCODE_LSHIFT:         return K_LSHIFT;
		case SDL_SCANCODE_RCTRL:          return K_RCTRL;
		case SDL_SCANCODE_LCTRL:          return K_LCTRL;
		case SDL_SCANCODE_RALT:           return K_RALT;
		case SDL_SCANCODE_LALT:           return K_LALT;
		case SDL_SCANCODE_LGUI:           return K_LSUPER;     // Left "Windows" key
		case SDL_SCANCODE_RGUI:           return K_RSUPER;     // Right "Windows" key
		case SDL_SCANCODE_MODE:           return K_MODE;       // "Alt Gr" key

		// Miscellaneous function keys
		case SDL_SCANCODE_HELP:           return K_HELP;
		case SDL_SCANCODE_PRINTSCREEN:    return K_PRINT;
		case SDL_SCANCODE_SYSREQ:         return K_SYSREQ;
		case SDL_SCANCODE_APPLICATION:    return K_MENU;
		case SDL_SCANCODE_MENU:           return K_MENU;
		case SDL_SCANCODE_POWER:          return K_POWER;      // Power Macintosh power key
		case SDL_SCANCODE_UNDO:           return K_UNDO;

		// Application Control keys
		case SDL_SCANCODE_AC_BACK:        return K_BACK;

		default:                          return K_UNKNOWN;
		}
	}();

	// Handle keys that don't have a key code but do have a scan code.
	if (key == K_UNKNOWN) {
		// Assume it is a Japanese keyboard and check
		// scancode to recognize a few japanese
		// specific keys for which SDL does not have an
		// SDL_Keycode keysym definition.
		switch (sdlscancode) {
		// Keys found on Japanese keyboards:
		case 49:
			key = K_ZENKAKU_HENKAKU;
			break;
		case 129:
			key = K_HENKAN_MODE;
			break;
		//case 131:
		//	key = K_MUHENKAN;
		//	break;
		case 208:
			key = K_HIRAGANA_KATAKANA;
			break;
		// Keys found on Korean keyboards:
		case 56:
			// On Korean keyboard this code is used for R-ALT key but SDL does
			// not seem to recognize it, as reported by Miso Kim.
			key = K_RALT;
			break;
		default:
			break; // nothing, silence compiler warning
		}
	}

	// Apply modifiers.
	if (mod & KMOD_CTRL) {
		key  = combine(key,  KM_CTRL);
		scan = combine(scan, KM_CTRL);
	}
	if (mod & KMOD_SHIFT) {
		key  = combine(key,  KM_SHIFT);
		scan = combine(scan, KM_SHIFT);
	}
	if (mod & KMOD_ALT) {
		key  = combine(key,  KM_ALT);
		scan = combine(scan, KM_ALT);
	}
	if (mod & KMOD_GUI) {
		key  = combine(key,  KM_META);
		scan = combine(scan, KM_META);
	}
	if (mod & KMOD_MODE) {
		key  = combine(key,  KM_MODE);
		scan = combine(scan, KM_MODE);
	}

	if (release) {
		key  = combine(key,  KD_RELEASE);
		scan = combine(scan, KD_RELEASE);
	}
	return {key, scan};
}

std::string getName(KeyCode keyCode)
{
	std::string result;
	for (const auto& [name, code] : keys) {
		if (code == (keyCode & K_MASK)) {
			result = name;
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

} // namespace openmsx::Keys
