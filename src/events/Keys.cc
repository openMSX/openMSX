#include "Keys.hh"
#include "StringOp.hh"
#include "cstd.hh"
#include "ranges.hh"
#include <array>

using std::string;
using std::string_view;

namespace openmsx::Keys {

// can be std::pair in C++20
struct P {
	constexpr P(string_view s, KeyCode k)
		: first(s), second(k) {}

	// Needed for gcc-6.3. Compiler bug?
	constexpr P& operator=(const P& o) {
		first = o.first;
		second = o.second;
		return *this;
	}
	constexpr P(const P& p) = default;

	string_view first;
	KeyCode second;
};

struct CmpKeys {
	// for cstd::sort
	constexpr bool operator()(const P& x, const P& y) const {
		return x.first < y.first; // shortcut: no need to ignore case
	}

	// for lower_bound()
	bool operator()(const P& x, string_view y) const {
		StringOp::caseless cmp;
		return cmp(x.first, y);
	}
};

#ifdef _MSC_VER
  // Workaround visual studio compiler bug, see
  //   https://developercommunity.visualstudio.com/content/problem/429980/wrong-code-with-constexpr-on-msvc-1916.html
  // Disable compile-time sorting on msvc until that bug gets solved.
  #define KEYS_CONSTEXPR
  #define KEYS_SORT ranges::sort
#else
  #define KEYS_CONSTEXPR constexpr
  #define KEYS_SORT cstd::sort
#endif
static KEYS_CONSTEXPR auto getSortedKeys()
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
	KEYS_SORT(keys, CmpKeys());
	return keys;
}

static KEYS_CONSTEXPR auto keys = getSortedKeys();

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
	KeyCode key;
	switch (sdlkeycode) {
	case SDLK_BACKSPACE:      key = K_BACKSPACE;         break;
	case SDLK_TAB:            key = K_TAB;               break;
	case SDLK_CLEAR:          key = K_CLEAR;             break;
	case SDLK_RETURN:         key = K_RETURN;            break;
	case SDLK_PAUSE:          key = K_PAUSE;             break;
	case SDLK_ESCAPE:         key = K_ESCAPE;            break;
	case SDLK_SPACE:          key = K_SPACE;             break;
	case SDLK_EXCLAIM:        key = K_EXCLAIM;           break;
	case SDLK_QUOTEDBL:       key = K_QUOTEDBL;          break;
	case SDLK_HASH:           key = K_HASH;              break;
	case SDLK_DOLLAR:         key = K_DOLLAR;            break;
	case SDLK_AMPERSAND:      key = K_AMPERSAND;         break;
	case SDLK_QUOTE:          key = K_QUOTE;             break;
	case SDLK_LEFTPAREN:      key = K_LEFTPAREN;         break;
	case SDLK_RIGHTPAREN:     key = K_RIGHTPAREN;        break;
	case SDLK_ASTERISK:       key = K_ASTERISK;          break;
	case SDLK_PLUS:           key = K_PLUS;              break;
	case SDLK_COMMA:          key = K_COMMA;             break;
	case SDLK_MINUS:          key = K_MINUS;             break;
	case SDLK_PERIOD:         key = K_PERIOD;            break;
	case SDLK_SLASH:          key = K_SLASH;             break;
	case SDLK_0:              key = K_0;                 break;
	case SDLK_1:              key = K_1;                 break;
	case SDLK_2:              key = K_2;                 break;
	case SDLK_3:              key = K_3;                 break;
	case SDLK_4:              key = K_4;                 break;
	case SDLK_5:              key = K_5;                 break;
	case SDLK_6:              key = K_6;                 break;
	case SDLK_7:              key = K_7;                 break;
	case SDLK_8:              key = K_8;                 break;
	case SDLK_9:              key = K_9;                 break;
	case SDLK_COLON:          key = K_COLON;             break;
	case SDLK_SEMICOLON:      key = K_SEMICOLON;         break;
	case SDLK_LESS:           key = K_LESS;              break;
	case SDLK_EQUALS:         key = K_EQUALS;            break;
	case SDLK_GREATER:        key = K_GREATER;           break;
	case SDLK_QUESTION:       key = K_QUESTION;          break;
	case SDLK_AT:             key = K_AT;                break;

	case SDLK_LEFTBRACKET:    key = K_LEFTBRACKET;       break;
	case SDLK_BACKSLASH:      key = K_BACKSLASH;         break;
	case SDLK_RIGHTBRACKET:   key = K_RIGHTBRACKET;      break;
	case SDLK_CARET:          key = K_CARET;             break;
	case SDLK_UNDERSCORE:     key = K_UNDERSCORE;        break;
	case SDLK_BACKQUOTE:      key = K_BACKQUOTE;         break;
	case SDLK_a:              key = K_A;                 break;
	case SDLK_b:              key = K_B;                 break;
	case SDLK_c:              key = K_C;                 break;
	case SDLK_d:              key = K_D;                 break;
	case SDLK_e:              key = K_E;                 break;
	case SDLK_f:              key = K_F;                 break;
	case SDLK_g:              key = K_G;                 break;
	case SDLK_h:              key = K_H;                 break;
	case SDLK_i:              key = K_I;                 break;
	case SDLK_j:              key = K_J;                 break;
	case SDLK_k:              key = K_K;                 break;
	case SDLK_l:              key = K_L;                 break;
	case SDLK_m:              key = K_M;                 break;
	case SDLK_n:              key = K_N;                 break;
	case SDLK_o:              key = K_O;                 break;
	case SDLK_p:              key = K_P;                 break;
	case SDLK_q:              key = K_Q;                 break;
	case SDLK_r:              key = K_R;                 break;
	case SDLK_s:              key = K_S;                 break;
	case SDLK_t:              key = K_T;                 break;
	case SDLK_u:              key = K_U;                 break;
	case SDLK_v:              key = K_V;                 break;
	case SDLK_w:              key = K_W;                 break;
	case SDLK_x:              key = K_X;                 break;
	case SDLK_y:              key = K_Y;                 break;
	case SDLK_z:              key = K_Z;                 break;
	case SDLK_DELETE:         key = K_DELETE;            break;

	// Numeric keypad
	case SDLK_KP_0:           key = K_KP0;               break;
	case SDLK_KP_1:           key = K_KP1;               break;
	case SDLK_KP_2:           key = K_KP2;               break;
	case SDLK_KP_3:           key = K_KP3;               break;
	case SDLK_KP_4:           key = K_KP4;               break;
	case SDLK_KP_5:           key = K_KP5;               break;
	case SDLK_KP_6:           key = K_KP6;               break;
	case SDLK_KP_7:           key = K_KP7;               break;
	case SDLK_KP_8:           key = K_KP8;               break;
	case SDLK_KP_9:           key = K_KP9;               break;
	case SDLK_KP_PERIOD:      key = K_KP_PERIOD;         break;
	case SDLK_KP_DIVIDE:      key = K_KP_DIVIDE;         break;
	case SDLK_KP_MULTIPLY:    key = K_KP_MULTIPLY;       break;
	case SDLK_KP_MINUS:       key = K_KP_MINUS;          break;
	case SDLK_KP_PLUS:        key = K_KP_PLUS;           break;
	case SDLK_KP_ENTER:       key = K_KP_ENTER;          break;
	case SDLK_KP_EQUALS:      key = K_KP_EQUALS;         break;

	// Arrows + Home/End pad
	case SDLK_UP:             key = K_UP;                break;
	case SDLK_DOWN:           key = K_DOWN;              break;
	case SDLK_RIGHT:          key = K_RIGHT;             break;
	case SDLK_LEFT:           key = K_LEFT;              break;
	case SDLK_INSERT:         key = K_INSERT;            break;
	case SDLK_HOME:           key = K_HOME;              break;
	case SDLK_END:            key = K_END;               break;
	case SDLK_PAGEUP:         key = K_PAGEUP;            break;
	case SDLK_PAGEDOWN:       key = K_PAGEDOWN;          break;

	// Function keys
	case SDLK_F1:             key = K_F1;                break;
	case SDLK_F2:             key = K_F2;                break;
	case SDLK_F3:             key = K_F3;                break;
	case SDLK_F4:             key = K_F4;                break;
	case SDLK_F5:             key = K_F5;                break;
	case SDLK_F6:             key = K_F6;                break;
	case SDLK_F7:             key = K_F7;                break;
	case SDLK_F8:             key = K_F8;                break;
	case SDLK_F9:             key = K_F9;                break;
	case SDLK_F10:            key = K_F10;               break;
	case SDLK_F11:            key = K_F11;               break;
	case SDLK_F12:            key = K_F12;               break;
	case SDLK_F13:            key = K_F13;               break;
	case SDLK_F14:            key = K_F14;               break;
	case SDLK_F15:            key = K_F15;               break;
	case SDLK_F16:            key = K_F16;               break;
	case SDLK_F17:            key = K_F17;               break;
	case SDLK_F18:            key = K_F18;               break;
	case SDLK_F19:            key = K_F19;               break;
	case SDLK_F20:            key = K_F20;               break;
	case SDLK_F21:            key = K_F21;               break;
	case SDLK_F22:            key = K_F22;               break;
	case SDLK_F23:            key = K_F23;               break;
	case SDLK_F24:            key = K_F24;               break;

	// Key state modifier keys
	case SDLK_NUMLOCKCLEAR:   key = K_NUMLOCK;           break;
	case SDLK_CAPSLOCK:       key = K_CAPSLOCK;          break;
	case SDLK_SCROLLLOCK:     key = K_SCROLLLOCK;        break;
	case SDLK_RSHIFT:         key = K_RSHIFT;            break;
	case SDLK_LSHIFT:         key = K_LSHIFT;            break;
	case SDLK_RCTRL:          key = K_RCTRL;             break;
	case SDLK_LCTRL:          key = K_LCTRL;             break;
	case SDLK_RALT:           key = K_RALT;              break;
	case SDLK_LALT:           key = K_LALT;              break;
//	case SDLK_RMETA:          key = K_RMETA;             break;
//	case SDLK_LMETA:          key = K_LMETA;             break;
	case SDLK_LGUI:           key = K_LSUPER;            break; // Left "Windows" key
	case SDLK_RGUI:           key = K_RSUPER;            break; // Right "Windows" key
	case SDLK_MODE:           key = K_MODE;              break; // "Alt Gr" key
//	case SDLK_COMPOSE:        key = K_COMPOSE;           break; // Multi-key compose key

	// Miscellaneous function keys
	case SDLK_HELP:           key = K_HELP;              break;
	case SDLK_PRINTSCREEN:    key = K_PRINT;             break;
	case SDLK_SYSREQ:         key = K_SYSREQ;            break;
//	case SDLK_BREAK:          key = K_BREAK;             break;
	case SDLK_APPLICATION:    key = K_MENU;              break;
	case SDLK_MENU:           key = K_MENU;              break;
	case SDLK_POWER:          key = K_POWER;             break; // Power Macintosh power key
//	case SDLK_EURO:           key = K_EURO;              break; // Some european keyboards
	case SDLK_UNDO:           key = K_UNDO;              break;

	// Application Control keys
	case SDLK_AC_BACK:        key = K_BACK;              break;

	default:                  key = K_UNKNOWN;           break;
	}

	KeyCode scan;
	switch (sdlscancode) {
	case SDL_SCANCODE_BACKSPACE:      scan = K_BACKSPACE;         break;
	case SDL_SCANCODE_TAB:            scan = K_TAB;               break;
	case SDL_SCANCODE_CLEAR:          scan = K_CLEAR;             break;
	case SDL_SCANCODE_RETURN:         scan = K_RETURN;            break;
	case SDL_SCANCODE_PAUSE:          scan = K_PAUSE;             break;
	case SDL_SCANCODE_ESCAPE:         scan = K_ESCAPE;            break;
	case SDL_SCANCODE_SPACE:          scan = K_SPACE;             break;
	case SDL_SCANCODE_APOSTROPHE:     scan = K_QUOTE;             break;
	case SDL_SCANCODE_COMMA:          scan = K_COMMA;             break;
	case SDL_SCANCODE_MINUS:          scan = K_MINUS;             break;
	case SDL_SCANCODE_PERIOD:         scan = K_PERIOD;            break;
	case SDL_SCANCODE_SLASH:          scan = K_SLASH;             break;
	case SDL_SCANCODE_0:              scan = K_0;                 break;
	case SDL_SCANCODE_1:              scan = K_1;                 break;
	case SDL_SCANCODE_2:              scan = K_2;                 break;
	case SDL_SCANCODE_3:              scan = K_3;                 break;
	case SDL_SCANCODE_4:              scan = K_4;                 break;
	case SDL_SCANCODE_5:              scan = K_5;                 break;
	case SDL_SCANCODE_6:              scan = K_6;                 break;
	case SDL_SCANCODE_7:              scan = K_7;                 break;
	case SDL_SCANCODE_8:              scan = K_8;                 break;
	case SDL_SCANCODE_9:              scan = K_9;                 break;
	case SDL_SCANCODE_SEMICOLON:      scan = K_SEMICOLON;         break;
	case SDL_SCANCODE_EQUALS:         scan = K_EQUALS;            break;

	case SDL_SCANCODE_LEFTBRACKET:    scan = K_LEFTBRACKET;       break;
	case SDL_SCANCODE_BACKSLASH:      scan = K_BACKSLASH;         break;
	case SDL_SCANCODE_RIGHTBRACKET:   scan = K_RIGHTBRACKET;      break;
	case SDL_SCANCODE_GRAVE:          scan = K_BACKQUOTE;         break;
	case SDL_SCANCODE_A:              scan = K_A;                 break;
	case SDL_SCANCODE_B:              scan = K_B;                 break;
	case SDL_SCANCODE_C:              scan = K_C;                 break;
	case SDL_SCANCODE_D:              scan = K_D;                 break;
	case SDL_SCANCODE_E:              scan = K_E;                 break;
	case SDL_SCANCODE_F:              scan = K_F;                 break;
	case SDL_SCANCODE_G:              scan = K_G;                 break;
	case SDL_SCANCODE_H:              scan = K_H;                 break;
	case SDL_SCANCODE_I:              scan = K_I;                 break;
	case SDL_SCANCODE_J:              scan = K_J;                 break;
	case SDL_SCANCODE_K:              scan = K_K;                 break;
	case SDL_SCANCODE_L:              scan = K_L;                 break;
	case SDL_SCANCODE_M:              scan = K_M;                 break;
	case SDL_SCANCODE_N:              scan = K_N;                 break;
	case SDL_SCANCODE_O:              scan = K_O;                 break;
	case SDL_SCANCODE_P:              scan = K_P;                 break;
	case SDL_SCANCODE_Q:              scan = K_Q;                 break;
	case SDL_SCANCODE_R:              scan = K_R;                 break;
	case SDL_SCANCODE_S:              scan = K_S;                 break;
	case SDL_SCANCODE_T:              scan = K_T;                 break;
	case SDL_SCANCODE_U:              scan = K_U;                 break;
	case SDL_SCANCODE_V:              scan = K_V;                 break;
	case SDL_SCANCODE_W:              scan = K_W;                 break;
	case SDL_SCANCODE_X:              scan = K_X;                 break;
	case SDL_SCANCODE_Y:              scan = K_Y;                 break;
	case SDL_SCANCODE_Z:              scan = K_Z;                 break;
	case SDL_SCANCODE_DELETE:         scan = K_DELETE;            break;

	// Numeric keypad
	case SDL_SCANCODE_KP_0:           scan = K_KP0;               break;
	case SDL_SCANCODE_KP_1:           scan = K_KP1;               break;
	case SDL_SCANCODE_KP_2:           scan = K_KP2;               break;
	case SDL_SCANCODE_KP_3:           scan = K_KP3;               break;
	case SDL_SCANCODE_KP_4:           scan = K_KP4;               break;
	case SDL_SCANCODE_KP_5:           scan = K_KP5;               break;
	case SDL_SCANCODE_KP_6:           scan = K_KP6;               break;
	case SDL_SCANCODE_KP_7:           scan = K_KP7;               break;
	case SDL_SCANCODE_KP_8:           scan = K_KP8;               break;
	case SDL_SCANCODE_KP_9:           scan = K_KP9;               break;
	case SDL_SCANCODE_KP_PERIOD:      scan = K_KP_PERIOD;         break;
	case SDL_SCANCODE_KP_DIVIDE:      scan = K_KP_DIVIDE;         break;
	case SDL_SCANCODE_KP_MULTIPLY:    scan = K_KP_MULTIPLY;       break;
	case SDL_SCANCODE_KP_MINUS:       scan = K_KP_MINUS;          break;
	case SDL_SCANCODE_KP_PLUS:        scan = K_KP_PLUS;           break;
	case SDL_SCANCODE_KP_ENTER:       scan = K_KP_ENTER;          break;
	case SDL_SCANCODE_KP_EQUALS:      scan = K_KP_EQUALS;         break;

	// Arrows + Home/End pad
	case SDL_SCANCODE_UP:             scan = K_UP;                break;
	case SDL_SCANCODE_DOWN:           scan = K_DOWN;              break;
	case SDL_SCANCODE_RIGHT:          scan = K_RIGHT;             break;
	case SDL_SCANCODE_LEFT:           scan = K_LEFT;              break;
	case SDL_SCANCODE_INSERT:         scan = K_INSERT;            break;
	case SDL_SCANCODE_HOME:           scan = K_HOME;              break;
	case SDL_SCANCODE_END:            scan = K_END;               break;
	case SDL_SCANCODE_PAGEUP:         scan = K_PAGEUP;            break;
	case SDL_SCANCODE_PAGEDOWN:       scan = K_PAGEDOWN;          break;

	// Function keys
	case SDL_SCANCODE_F1:             scan = K_F1;                break;
	case SDL_SCANCODE_F2:             scan = K_F2;                break;
	case SDL_SCANCODE_F3:             scan = K_F3;                break;
	case SDL_SCANCODE_F4:             scan = K_F4;                break;
	case SDL_SCANCODE_F5:             scan = K_F5;                break;
	case SDL_SCANCODE_F6:             scan = K_F6;                break;
	case SDL_SCANCODE_F7:             scan = K_F7;                break;
	case SDL_SCANCODE_F8:             scan = K_F8;                break;
	case SDL_SCANCODE_F9:             scan = K_F9;                break;
	case SDL_SCANCODE_F10:            scan = K_F10;               break;
	case SDL_SCANCODE_F11:            scan = K_F11;               break;
	case SDL_SCANCODE_F12:            scan = K_F12;               break;
	case SDL_SCANCODE_F13:            scan = K_F13;               break;
	case SDL_SCANCODE_F14:            scan = K_F14;               break;
	case SDL_SCANCODE_F15:            scan = K_F15;               break;
	case SDL_SCANCODE_F16:            scan = K_F16;               break;
	case SDL_SCANCODE_F17:            scan = K_F17;               break;
	case SDL_SCANCODE_F18:            scan = K_F18;               break;
	case SDL_SCANCODE_F19:            scan = K_F19;               break;
	case SDL_SCANCODE_F20:            scan = K_F20;               break;
	case SDL_SCANCODE_F21:            scan = K_F21;               break;
	case SDL_SCANCODE_F22:            scan = K_F22;               break;
	case SDL_SCANCODE_F23:            scan = K_F23;               break;
	case SDL_SCANCODE_F24:            scan = K_F24;               break;

	// Key state modifier keys
	case SDL_SCANCODE_NUMLOCKCLEAR:   scan = K_NUMLOCK;           break;
	case SDL_SCANCODE_CAPSLOCK:       scan = K_CAPSLOCK;          break;
	case SDL_SCANCODE_SCROLLLOCK:     scan = K_SCROLLLOCK;        break;
	case SDL_SCANCODE_RSHIFT:         scan = K_RSHIFT;            break;
	case SDL_SCANCODE_LSHIFT:         scan = K_LSHIFT;            break;
	case SDL_SCANCODE_RCTRL:          scan = K_RCTRL;             break;
	case SDL_SCANCODE_LCTRL:          scan = K_LCTRL;             break;
	case SDL_SCANCODE_RALT:           scan = K_RALT;              break;
	case SDL_SCANCODE_LALT:           scan = K_LALT;              break;
	case SDL_SCANCODE_LGUI:           scan = K_LSUPER;            break; // Left "Windows" key
	case SDL_SCANCODE_RGUI:           scan = K_RSUPER;            break; // Right "Windows" key
	case SDL_SCANCODE_MODE:           scan = K_MODE;              break; // "Alt Gr" key

	// Miscellaneous function keys
	case SDL_SCANCODE_HELP:           scan = K_HELP;              break;
	case SDL_SCANCODE_PRINTSCREEN:    scan = K_PRINT;             break;
	case SDL_SCANCODE_SYSREQ:         scan = K_SYSREQ;            break;
	case SDL_SCANCODE_APPLICATION:    scan = K_MENU;              break;
	case SDL_SCANCODE_MENU:           scan = K_MENU;              break;
	case SDL_SCANCODE_POWER:          scan = K_POWER;             break; // Power Macintosh power key
	case SDL_SCANCODE_UNDO:           scan = K_UNDO;              break;

	// Application Control keys
	case SDL_SCANCODE_AC_BACK:        scan = K_BACK;              break;

	default:                          scan = K_UNKNOWN;           break;
	}

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

string getName(KeyCode keyCode)
{
	string result;
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
