#include "Keys.hh"
#include "StringOp.hh"
#include "stl.hh"
#include <algorithm>
#include <vector>
#include <utility>
#include "cstd.hh"

using std::string;

namespace openmsx {

namespace Keys {

#if HAS_CPP14_CONSTEXPR

// can be std::pair in C++17
struct P {
	constexpr P(cstd::string s, KeyCode k)
		: first(s), second(k) {}

	// Needed for gcc-6.3. Compiler bug?
	constexpr P& operator=(const P& o) {
		first = o.first;
		second = o.second;
		return *this;
	}

	cstd::string first;
	KeyCode second;
};

struct CmpKeys {
	// for cstd::sort
	constexpr bool operator()(const P& x, const P& y) const {
		return x.first < y.first; // shortcut: no need to ignore case
	}

	// for std::lower_bound
	bool operator()(const P& x, string_view y) const {
		StringOp::caseless cmp;
		return cmp(x.first, y);
	}
};

static constexpr auto getSortedKeys()
{
	auto keys = cstd::array_of<P>(
#else

using P = std::pair<string_view, KeyCode>;
using CmpKeys = CmpTupleElement<0, StringOp::caseless>;

static std::vector<P> getSortedKeys()
{
	auto keys = std::vector<P>{
#endif
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

		// Key state modifier keys
		P("NUMLOCK",	K_NUMLOCK),
		P("CAPSLOCK",	K_CAPSLOCK),
		P("SCROLLOCK",	K_SCROLLOCK),
		P("RSHIFT",	K_RSHIFT),
		P("LSHIFT",	K_LSHIFT),
		P("RCTRL",	K_RCTRL),
		P("LCTRL",	K_LCTRL),
		P("RALT",	K_RALT),
		P("LALT",	K_LALT),
		P("RMETA",	K_RMETA),
		P("LMETA",	K_LMETA),
		P("LSUPER",	K_LSUPER),	// Left "Windows" key
		P("RSUPER",	K_RSUPER),	// Right "Windows" key
		P("RMODE",	K_MODE),	// "Alt Gr" key
		P("COMPOSE",	K_COMPOSE),	// Multi-key compose key

		// Miscellaneous function keys
		P("HELP",	K_HELP),
		P("PRINT",	K_PRINT),
		P("SYSREQ",	K_SYSREQ),
		P("BREAK",	K_BREAK),
		P("MENU",	K_MENU),
		P("POWER",	K_POWER),	// Power Macintosh power key
		P("EURO",	K_EURO),	// Some european keyboards
		P("UNDO",	K_UNDO),

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
#if HAS_CPP14_CONSTEXPR
	);
	cstd::sort(cstd::begin(keys), cstd::end(keys), CmpKeys());
	return keys;
}
#else
	};
	std::sort(begin(keys), end(keys), CmpKeys());
	return keys;
}
#endif

static CONSTEXPR auto keys = getSortedKeys();

KeyCode getCode(string_view name)
{
	auto result = static_cast<KeyCode>(0);
	string_view::size_type lastPos = 0;
	while (lastPos != string_view::npos) {
		auto pos = name.substr(lastPos).find_first_of(",+/");
		auto part = (pos != string_view::npos)
		          ? name.substr(lastPos, pos)
		          : name.substr(lastPos);
		auto it = std::lower_bound(begin(keys), end(keys), part, CmpKeys());
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
		lastPos = (pos != string_view::npos)
		        ? lastPos + pos + 1
		        : string_view::npos;
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
	string result;
	for (auto& p : keys) {
		if (p.second == (keyCode & K_MASK)) {
			result = string_view(p.first).str();
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
