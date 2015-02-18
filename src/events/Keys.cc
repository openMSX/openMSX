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
		{ "RMETA",	K_RMETA },
		{ "LMETA",	K_LMETA },
		{ "LSUPER",	K_LSUPER },	// Left "Windows" key
		{ "RSUPER",	K_RSUPER },	// Right "Windows" key
		{ "RMODE",	K_MODE },	// "Alt Gr" key
		{ "COMPOSE",	K_COMPOSE },	// Multi-key compose key

		// Miscellaneous function keys
		{ "HELP",	K_HELP },
		{ "PRINT",	K_PRINT },
		{ "SYSREQ",	K_SYSREQ },
		{ "BREAK",	K_BREAK },
		{ "MENU",	K_MENU },
		{ "POWER",	K_POWER },	// Power Macintosh power key
		{ "EURO",	K_EURO },	// Some european keyboards
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

KeyCode getCode(SDLKey key, SDLMod mod, Uint8 scancode, bool release)
{
	auto result = static_cast<KeyCode>(key);
	if (result == 0) {
		// Assume it is a Japanese keyboard and check
		// scancode to recognize a few japanese
		// specific keys for which SDL does not have an
		// SDLKey keysym definition.
		switch (scancode) {
		case 49:
			result = static_cast<KeyCode>(K_ZENKAKU_HENKAKU);
			break;
		case 129:
			result = static_cast<KeyCode>(K_HENKAN_MODE);
			break;
		case 131:
			result = static_cast<KeyCode>(K_MUHENKAN);
			break;
		case 208:
			result = static_cast<KeyCode>(K_HIRAGANA_KATAKANA);
			break;
		}
	}
	if (result == 0) {
		// Assume it is a Korean keyboard and check
		// for scancode 56; on Korean keyboard it is used
		// for R-ALT key but SDL does not seem to recognize it,
		// as reported by Miso Kim
		switch (scancode) {
		case 56:
			result = static_cast<KeyCode>(K_RALT);
			break;
		}
	}	
	if (mod & KMOD_CTRL) {
		result = static_cast<KeyCode>(result | KM_CTRL);
	}
	if (mod & KMOD_SHIFT) {
		result = static_cast<KeyCode>(result | KM_SHIFT);
	}
	if (mod & KMOD_ALT) {
		result = static_cast<KeyCode>(result | KM_ALT);
	}
	if (mod & KMOD_META) {
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
