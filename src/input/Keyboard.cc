#include "Keyboard.hh"

#include "CommandController.hh"
#include "CommandException.hh"
#include "DeviceConfig.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "InputEventFactory.hh"
#include "MSXEventDistributor.hh"
#include "MSXMotherBoard.hh"
#include "ReverseManager.hh"
#include "SDLKey.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"
#include "TclArgParser.hh"
#include "TclObject.hh"
#include "UnicodeKeymap.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "serialize_stl.hh"

#include "enumerate.hh"
#include "one_of.hh"
#include "outer.hh"
#include "ranges.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "utf8_checked.hh"
#include "utf8_unchecked.hh"
#include "xrange.hh"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <ranges>
#include <type_traits>

namespace openmsx {

// How does the CAPSLOCK key behave?
#ifdef __APPLE__
// See the comments in this issue:
//    https://github.com/openMSX/openMSX/issues/1261
// Basically it means on apple:
//   when the host capslock key is pressed,       SDL sends capslock-pressed
//   when the host capslock key is released,      SDL sends nothing
//   when the host capslock key is pressed again, SDL sends capslock-released
//   when the host capslock key is released,      SDL sends nothing
static constexpr bool SANE_CAPSLOCK_BEHAVIOR = false;
#else
// We get sane capslock events from SDL:
//  when the host capslock key is pressed,  SDL sends capslock-pressed
//  when the host capslock key is released, SDL sends capslock-released
static constexpr bool SANE_CAPSLOCK_BEHAVIOR = true;
#endif


static constexpr uint8_t TRY_AGAIN = 0x80; // see pressAscii()

using KeyInfo = UnicodeKeymap::KeyInfo;

class KeyMatrixState final : public StateChange
{
public:
	KeyMatrixState() = default; // for serialize
	KeyMatrixState(EmuTime time_, uint8_t row_, uint8_t press_, uint8_t release_)
		: StateChange(time_)
		, row(row_), press(press_), release(release_)
	{
		// disallow useless events
		assert((press != 0) || (release != 0));
		// avoid confusion about what happens when some bits are both
		// set and reset (in other words: don't rely on order of and-
		// and or-operations)
		assert((press & release) == 0);
	}
	[[nodiscard]] uint8_t getRow()     const { return row; }
	[[nodiscard]] uint8_t getPress()   const { return press; }
	[[nodiscard]] uint8_t getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("row",     row,
		             "press",   press,
		             "release", release);
	}
private:
	uint8_t row, press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, KeyMatrixState, "KeyMatrixState");


static constexpr array_with_enum_index<Keyboard::Matrix, std::string_view> defaultKeymapForMatrix = {
	"int", // Matrix::MSX
	"svi", // Matrix::SVI
	"cvjoy", // Matrix::CVJOY
	"sega_int", // Matrix::SEGA
};

using ModifierArray = array_with_enum_index<UnicodeKeymap::KeyInfo::Modifier, KeyMatrixPosition>;
static constexpr array_with_enum_index<Keyboard::Matrix, ModifierArray> modifierPosForMatrix = {
	ModifierArray{ // Matrix::MSX
		KeyMatrixPosition(6, 0), // SHIFT
		KeyMatrixPosition(6, 1), // CTRL
		KeyMatrixPosition(6, 2), // GRAPH
		KeyMatrixPosition(6, 3), // CAPS
		KeyMatrixPosition(6, 4), // CODE
	},
	ModifierArray{ // Matrix::SVI
		KeyMatrixPosition(6, 0), // SHIFT
		KeyMatrixPosition(6, 1), // CTRL
		KeyMatrixPosition(6, 2), // LGRAPH
		KeyMatrixPosition(8, 3), // CAPS
		KeyMatrixPosition(6, 3), // RGRAPH
	},
	ModifierArray{ // Matrix::CVJOY
	},
	ModifierArray{ // Matrix::SEGA
		KeyMatrixPosition(13, 3), // SHIFT
		KeyMatrixPosition(13, 2), // CTRL
		KeyMatrixPosition(13, 1), // GRAPH
		KeyMatrixPosition(),      // CAPS
		KeyMatrixPosition( 0, 4), // ENG/DIER'S
	},
};

/** Keyboard bindings ****************************************/

struct MsxKeyScanMapping {
	KeyMatrixPosition msx;
	std::array<SDL_Keycode, 3> hostKeyCodes;
	std::array<SDL_Scancode, 3> hostScanCodes;
};

template<typename Proj>
static constexpr size_t count(std::span<const MsxKeyScanMapping> mapping, Proj proj)
{
	using Array = std::remove_cvref_t<decltype(std::invoke(proj, mapping[0]))>;
	using Code = typename Array::value_type;

	size_t result = 0;
	for (const auto& m : mapping) {
		for (const auto& c : std::invoke(proj, m)) {
			if (c != Code{}) ++result;
		}
	}
	return result;
}

template<typename GetMapping>
static constexpr auto extractKeyCodeMapping(GetMapping getMapping)
{
	constexpr auto mapping = getMapping();
	constexpr size_t N = count(mapping, &MsxKeyScanMapping::hostKeyCodes);
	std::array<KeyCodeMsxMapping, N> result;
	size_t i = 0;
	for (const auto& m : mapping) {
		for (const auto& k : m.hostKeyCodes) {
			if (k != SDLK_UNKNOWN) {
				result[i++] = {k, m.msx};
			}
		}
	}
	assert(i == N);
	std::ranges::sort(result, {}, &KeyCodeMsxMapping::hostKeyCode);
	return result;
}
template<typename GetMapping>
static constexpr auto extractScanCodeMapping(GetMapping getMapping)
{
	constexpr auto mapping = getMapping();
	constexpr size_t N = count(mapping, &MsxKeyScanMapping::hostScanCodes);
	std::array<ScanCodeMsxMapping, N> result;
	size_t i = 0;
	for (const auto& m : mapping) {
		for (const auto& k : m.hostScanCodes) {
			if (k != SDL_SCANCODE_UNKNOWN) {
				result[i++] = {k, m.msx};
			}
		}
	}
	assert(i == N);
	std::ranges::sort(result, {}, &ScanCodeMsxMapping::hostScanCode);
	return result;
}

static constexpr auto getMSXMapping()
{
	// MSX Key-Matrix table
	//
	// row/bit  7     6     5     4     3     2     1     0
	//       +-----+-----+-----+-----+-----+-----+-----+-----+
	//   0   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
	//   1   |  ;  |  ]  |  [  |  \  |  =  |  -  |  9  |  8  |
	//   2   |  B  |  A  | Acc |  /  |  .  |  ,  |  `  |  '  |
	//   3   |  J  |  I  |  H  |  G  |  F  |  E  |  D  |  C  |
	//   4   |  R  |  Q  |  P  |  O  |  N  |  M  |  L  |  K  |
	//   5   |  Z  |  Y  |  X  |  W  |  V  |  U  |  T  |  S  |
	//   6   |  F3 |  F2 |  F1 | code| caps|graph| ctrl|shift|
	//   7   | ret |selec|  bs | stop| tab | esc |  F5 |  F4 |
	//   8   |right| down|  up | left| del | ins | hom |space|
	//   9   |  4  |  3  |  2  |  1  |  0  |  /  |  +  |  *  |
	//  10   |  .  |  ,  |  -  |  9  |  8  |  7  |  6  |  5  |
	//  11   |     |     |     |     | 'NO'|     |'YES'|     |
	//       +-----+-----+-----+-----+-----+-----+-----+-----+
	using M = MsxKeyScanMapping;
	using P = KeyMatrixPosition;
	using K = std::array<SDL_Keycode, 3>;
	using S = std::array<SDL_Scancode, 3>;
	std::array mapping = {
		M{P{0x00}, K{SDLK_0},           S{SDL_SCANCODE_0}},
		M{P{0x01}, K{SDLK_1},           S{SDL_SCANCODE_1}},
		M{P{0x02}, K{SDLK_2},           S{SDL_SCANCODE_2}},
		M{P{0x03}, K{SDLK_3},           S{SDL_SCANCODE_3}},
		M{P{0x04}, K{SDLK_4},           S{SDL_SCANCODE_4}},
		M{P{0x05}, K{SDLK_5},           S{SDL_SCANCODE_5}},
		M{P{0x06}, K{SDLK_6},           S{SDL_SCANCODE_6}},
		M{P{0x07}, K{SDLK_7},           S{SDL_SCANCODE_7}},

		M{P{0x10}, K{SDLK_8},           S{SDL_SCANCODE_8}},
		M{P{0x11}, K{SDLK_9},           S{SDL_SCANCODE_9}},
		M{P{0x12}, K{SDLK_MINUS},       S{SDL_SCANCODE_MINUS}},
		M{P{0x13}, K{SDLK_EQUALS},      S{SDL_SCANCODE_EQUALS}},
		M{P{0x14}, K{SDLK_BACKSLASH},   S{SDL_SCANCODE_BACKSLASH, SDL_SCANCODE_INTERNATIONAL3}}, // on Japanese keyboard, the YEN = SDL_SCANCODE_INTERNATIONAL3, but RIGHTBRACKET = SDL_SCANCODE_BACKSLASH
		M{P{0x15}, K{SDLK_LEFTBRACKET}, S{SDL_SCANCODE_LEFTBRACKET}},
		M{P{0x16}, K{SDLK_RIGHTBRACKET},S{SDL_SCANCODE_RIGHTBRACKET}},
		M{P{0x17}, K{SDLK_SEMICOLON},   S{SDL_SCANCODE_SEMICOLON}},

		M{P{0x20}, K{SDLK_QUOTE},       S{SDL_SCANCODE_APOSTROPHE}},
		M{P{0x21}, K{SDLK_BACKQUOTE},   S{SDL_SCANCODE_GRAVE}},
		M{P{0x22}, K{SDLK_COMMA},       S{SDL_SCANCODE_COMMA}},
		M{P{0x23}, K{SDLK_PERIOD},      S{SDL_SCANCODE_PERIOD}},
		M{P{0x24}, K{SDLK_SLASH},       S{SDL_SCANCODE_SLASH}},
		M{P{0x25}, K{SDLK_RCTRL},       S{SDL_SCANCODE_RCTRL, SDL_SCANCODE_NONUSBACKSLASH}}, // Acc / BACKSLASH on Japanese keyboard
		M{P{0x26}, K{SDLK_a},           S{SDL_SCANCODE_A}},
		M{P{0x27}, K{SDLK_b},           S{SDL_SCANCODE_B}},

		M{P{0x30}, K{SDLK_c},           S{SDL_SCANCODE_C}},
		M{P{0x31}, K{SDLK_d},           S{SDL_SCANCODE_D}},
		M{P{0x32}, K{SDLK_e},           S{SDL_SCANCODE_E}},
		M{P{0x33}, K{SDLK_f},           S{SDL_SCANCODE_F}},
		M{P{0x34}, K{SDLK_g},           S{SDL_SCANCODE_G}},
		M{P{0x35}, K{SDLK_h},           S{SDL_SCANCODE_H}},
		M{P{0x36}, K{SDLK_i},           S{SDL_SCANCODE_I}},
		M{P{0x37}, K{SDLK_j},           S{SDL_SCANCODE_J}},

		M{P{0x40}, K{SDLK_k},           S{SDL_SCANCODE_K}},
		M{P{0x41}, K{SDLK_l},           S{SDL_SCANCODE_L}},
		M{P{0x42}, K{SDLK_m},           S{SDL_SCANCODE_M}},
		M{P{0x43}, K{SDLK_n},           S{SDL_SCANCODE_N}},
		M{P{0x44}, K{SDLK_o},           S{SDL_SCANCODE_O}},
		M{P{0x45}, K{SDLK_p},           S{SDL_SCANCODE_P}},
		M{P{0x46}, K{SDLK_q},           S{SDL_SCANCODE_Q}},
		M{P{0x47}, K{SDLK_r},           S{SDL_SCANCODE_R}},

		M{P{0x50}, K{SDLK_s},           S{SDL_SCANCODE_S}},
		M{P{0x51}, K{SDLK_t},           S{SDL_SCANCODE_T}},
		M{P{0x52}, K{SDLK_u},           S{SDL_SCANCODE_U}},
		M{P{0x53}, K{SDLK_v},           S{SDL_SCANCODE_V}},
		M{P{0x54}, K{SDLK_w},           S{SDL_SCANCODE_W}},
		M{P{0x55}, K{SDLK_x},           S{SDL_SCANCODE_X}},
		M{P{0x56}, K{SDLK_y},           S{SDL_SCANCODE_Y}},
		M{P{0x57}, K{SDLK_z},           S{SDL_SCANCODE_Z}},

		M{P{0x60}, K{SDLK_LSHIFT, SDLK_RSHIFT}, S{SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT}},
		M{P{0x61}, K{SDLK_LCTRL},       S{SDL_SCANCODE_LCTRL}},
		M{P{0x62}, K{SDLK_LALT},        S{SDL_SCANCODE_LALT}}, // GRAPH
		M{P{0x63}, K{SDLK_CAPSLOCK},    S{SDL_SCANCODE_CAPSLOCK}},
		M{P{0x64}, K{SDLK_RALT},        S{SDL_SCANCODE_RALT}}, // CODE
		M{P{0x65}, K{SDLK_F1},          S{SDL_SCANCODE_F1}},
		M{P{0x66}, K{SDLK_F2},          S{SDL_SCANCODE_F2}},
		M{P{0x67}, K{SDLK_F3},          S{SDL_SCANCODE_F3}},

		M{P{0x70}, K{SDLK_F4},          S{SDL_SCANCODE_F4}},
		M{P{0x71}, K{SDLK_F5},          S{SDL_SCANCODE_F5}},
		M{P{0x72}, K{SDLK_ESCAPE},      S{SDL_SCANCODE_ESCAPE}},
		M{P{0x73}, K{SDLK_TAB},         S{SDL_SCANCODE_TAB}},
		M{P{0x74}, K{SDLK_F8},          S{SDL_SCANCODE_F8}}, // STOP
		M{P{0x75}, K{SDLK_BACKSPACE},   S{SDL_SCANCODE_BACKSPACE}},
		M{P{0x76}, K{SDLK_F7},          S{SDL_SCANCODE_F7}}, // SELECT
		M{P{0x77}, K{SDLK_RETURN},      S{SDL_SCANCODE_RETURN}},

		M{P{0x80}, K{SDLK_SPACE},       S{SDL_SCANCODE_SPACE}},
		M{P{0x81}, K{SDLK_HOME},        S{SDL_SCANCODE_HOME}},
		M{P{0x82}, K{SDLK_INSERT},      S{SDL_SCANCODE_INSERT}},
		M{P{0x83}, K{SDLK_DELETE},      S{SDL_SCANCODE_DELETE}},
		M{P{0x84}, K{SDLK_LEFT},        S{SDL_SCANCODE_LEFT}},
		M{P{0x85}, K{SDLK_UP},          S{SDL_SCANCODE_UP}},
		M{P{0x86}, K{SDLK_DOWN},        S{SDL_SCANCODE_DOWN}},
		M{P{0x87}, K{SDLK_RIGHT},       S{SDL_SCANCODE_RIGHT}},

		M{P{0x90}, K{SDLK_KP_MULTIPLY}, S{SDL_SCANCODE_KP_MULTIPLY}},
		M{P{0x91}, K{SDLK_KP_PLUS},     S{SDL_SCANCODE_KP_PLUS}},
		M{P{0x92}, K{SDLK_KP_DIVIDE},   S{SDL_SCANCODE_KP_DIVIDE}},
		M{P{0x93}, K{SDLK_KP_0},        S{SDL_SCANCODE_KP_0}},
		M{P{0x94}, K{SDLK_KP_1},        S{SDL_SCANCODE_KP_1}},
		M{P{0x95}, K{SDLK_KP_2},        S{SDL_SCANCODE_KP_2}},
		M{P{0x96}, K{SDLK_KP_3},        S{SDL_SCANCODE_KP_3}},
		M{P{0x97}, K{SDLK_KP_4},        S{SDL_SCANCODE_KP_4}},

		M{P{0xA0}, K{SDLK_KP_5},        S{SDL_SCANCODE_KP_5}},
		M{P{0xA1}, K{SDLK_KP_6},        S{SDL_SCANCODE_KP_6}},
		M{P{0xA2}, K{SDLK_KP_7},        S{SDL_SCANCODE_KP_7}},
		M{P{0xA3}, K{SDLK_KP_8},        S{SDL_SCANCODE_KP_8}},
		M{P{0xA4}, K{SDLK_KP_9},        S{SDL_SCANCODE_KP_9}},
		M{P{0xA5}, K{SDLK_KP_MINUS},    S{SDL_SCANCODE_KP_MINUS}},
		M{P{0xA6}, K{SDLK_KP_COMMA},    S{SDL_SCANCODE_KP_COMMA}},
		M{P{0xA7}, K{SDLK_KP_PERIOD},   S{SDL_SCANCODE_KP_PERIOD}},

		M{P{0xB1}, K{SDLK_RGUI},        S{SDL_SCANCODE_RGUI}},
		M{P{0xB3}, K{SDLK_LGUI},        S{SDL_SCANCODE_LGUI}},
	};
	return mapping;
}

static constexpr auto getSVIMapping()
{
	// SVI Keyboard Matrix
	//
	// row/bit  7     6     5     4     3     2     1     0
	//       +-----+-----+-----+-----+-----+-----+-----+-----+
	//   0   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
	//   1   |  /  |  .  |  =  |  ,  |  '  |  :  |  9  |  8  |
	//   2   |  G  |  F  |  E  |  D  |  C  |  B  |  A  |  -  |
	//   3   |  O  |  N  |  M  |  L  |  K  |  J  |  I  |  H  |
	//   4   |  W  |  V  |  U  |  T  |  S  |  R  |  Q  |  P  |
	//   5   | UP  | BS  |  ]  |  \  |  [  |  Z  |  Y  |  X  |
	//   6   |LEFT |ENTER|STOP | ESC |RGRAP|LGRAP|CTRL |SHIFT|
	//   7   |DOWN | INS | CLS | F5  | F4  | F3  | F2  | F1  |
	//   8   |RIGHT|     |PRINT| SEL |CAPS | DEL | TAB |SPACE|
	//   9   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |  Numerical keypad
	//  10   |  ,  |  .  |  /  |  *  |  -  |  +  |  9  |  8  |   SVI-328 only
	//       +-----+-----+-----+-----+-----+-----+-----+-----+
	using M = MsxKeyScanMapping;
	using P = KeyMatrixPosition;
	using K = std::array<SDL_Keycode, 3>;
	using S = std::array<SDL_Scancode, 3>;
	std::array mapping = {
		M{P{0x00}, K{SDLK_0},           S{SDL_SCANCODE_0}},
		M{P{0x01}, K{SDLK_1},           S{SDL_SCANCODE_1}},
		M{P{0x02}, K{SDLK_2},           S{SDL_SCANCODE_2}},
		M{P{0x03}, K{SDLK_3},           S{SDL_SCANCODE_3}},
		M{P{0x04}, K{SDLK_4},           S{SDL_SCANCODE_4}},
		M{P{0x05}, K{SDLK_5},           S{SDL_SCANCODE_5}},
		M{P{0x06}, K{SDLK_6},           S{SDL_SCANCODE_6}},
		M{P{0x07}, K{SDLK_7},           S{SDL_SCANCODE_7}},

		M{P{0x10}, K{SDLK_8},           S{SDL_SCANCODE_8}},
		M{P{0x11}, K{SDLK_9},           S{SDL_SCANCODE_9}},
		M{P{0x12}, K{SDLK_SEMICOLON},   S{SDL_SCANCODE_SEMICOLON}},
		M{P{0x13}, K{SDLK_QUOTE},       S{SDL_SCANCODE_APOSTROPHE}},
		M{P{0x14}, K{SDLK_COMMA},       S{SDL_SCANCODE_COMMA}},
		M{P{0x15}, K{SDLK_EQUALS},      S{SDL_SCANCODE_EQUALS}},
		M{P{0x16}, K{SDLK_PERIOD},      S{SDL_SCANCODE_PERIOD}},
		M{P{0x17}, K{SDLK_SLASH},       S{SDL_SCANCODE_SLASH}},

		M{P{0x20}, K{SDLK_MINUS},       S{SDL_SCANCODE_MINUS}},
		M{P{0x21}, K{SDLK_a},           S{SDL_SCANCODE_A}},
		M{P{0x22}, K{SDLK_b},           S{SDL_SCANCODE_B}},
		M{P{0x23}, K{SDLK_c},           S{SDL_SCANCODE_C}},
		M{P{0x24}, K{SDLK_d},           S{SDL_SCANCODE_D}},
		M{P{0x25}, K{SDLK_e},           S{SDL_SCANCODE_E}},
		M{P{0x26}, K{SDLK_f},           S{SDL_SCANCODE_F}},
		M{P{0x27}, K{SDLK_g},           S{SDL_SCANCODE_G}},

		M{P{0x30}, K{SDLK_h},           S{SDL_SCANCODE_H}},
		M{P{0x31}, K{SDLK_i},           S{SDL_SCANCODE_I}},
		M{P{0x32}, K{SDLK_j},           S{SDL_SCANCODE_J}},
		M{P{0x33}, K{SDLK_k},           S{SDL_SCANCODE_K}},
		M{P{0x34}, K{SDLK_l},           S{SDL_SCANCODE_L}},
		M{P{0x35}, K{SDLK_m},           S{SDL_SCANCODE_M}},
		M{P{0x36}, K{SDLK_n},           S{SDL_SCANCODE_N}},
		M{P{0x37}, K{SDLK_o},           S{SDL_SCANCODE_O}},

		M{P{0x40}, K{SDLK_p},           S{SDL_SCANCODE_P}},
		M{P{0x41}, K{SDLK_q},           S{SDL_SCANCODE_Q}},
		M{P{0x42}, K{SDLK_r},           S{SDL_SCANCODE_R}},
		M{P{0x43}, K{SDLK_s},           S{SDL_SCANCODE_S}},
		M{P{0x44}, K{SDLK_t},           S{SDL_SCANCODE_T}},
		M{P{0x45}, K{SDLK_u},           S{SDL_SCANCODE_U}},
		M{P{0x46}, K{SDLK_v},           S{SDL_SCANCODE_V}},
		M{P{0x47}, K{SDLK_w},           S{SDL_SCANCODE_W}},

		M{P{0x50}, K{SDLK_x},           S{SDL_SCANCODE_X}},
		M{P{0x51}, K{SDLK_y},           S{SDL_SCANCODE_Y}},
		M{P{0x52}, K{SDLK_z},           S{SDL_SCANCODE_Z}},
		M{P{0x53}, K{SDLK_LEFTBRACKET}, S{SDL_SCANCODE_LEFTBRACKET}},
		M{P{0x54}, K{SDLK_BACKSLASH},   S{SDL_SCANCODE_BACKSLASH}},
		M{P{0x55}, K{SDLK_RIGHTBRACKET},S{SDL_SCANCODE_RIGHTBRACKET}},
		M{P{0x56}, K{SDLK_BACKSPACE},   S{SDL_SCANCODE_BACKSPACE}},
		M{P{0x57}, K{SDLK_UP},          S{SDL_SCANCODE_UP}},

		M{P{0x60}, K{SDLK_LSHIFT, SDLK_RSHIFT}, S{SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT}},
		M{P{0x61}, K{SDLK_LCTRL},       S{SDL_SCANCODE_LCTRL}},
		M{P{0x62}, K{SDLK_LALT},        S{SDL_SCANCODE_LALT}},
		M{P{0x63}, K{SDLK_RALT},        S{SDL_SCANCODE_RALT}},
		M{P{0x64}, K{SDLK_ESCAPE},      S{SDL_SCANCODE_ESCAPE}},
		M{P{0x65}, K{SDLK_F8},          S{SDL_SCANCODE_F8}}, // STOP
		M{P{0x66}, K{SDLK_RETURN},      S{SDL_SCANCODE_RETURN}},
		M{P{0x67}, K{SDLK_LEFT},        S{SDL_SCANCODE_LEFT}},

		M{P{0x70}, K{SDLK_F1},          S{SDL_SCANCODE_F1}},
		M{P{0x71}, K{SDLK_F2},          S{SDL_SCANCODE_F2}},
		M{P{0x72}, K{SDLK_F3},          S{SDL_SCANCODE_F3}},
		M{P{0x73}, K{SDLK_F4},          S{SDL_SCANCODE_F4}},
		M{P{0x74}, K{SDLK_F5},          S{SDL_SCANCODE_F5}},
		M{P{0x75}, K{SDLK_F6},          S{SDL_SCANCODE_F6}}, // CLS
		M{P{0x76}, K{SDLK_INSERT},      S{SDL_SCANCODE_INSERT}},
		M{P{0x77}, K{SDLK_DOWN},        S{SDL_SCANCODE_DOWN}},

		M{P{0x80}, K{SDLK_SPACE},       S{SDL_SCANCODE_SPACE}},
		M{P{0x81}, K{SDLK_TAB},         S{SDL_SCANCODE_TAB}},
		M{P{0x82}, K{SDLK_DELETE},      S{SDL_SCANCODE_DELETE}},
		M{P{0x83}, K{SDLK_CAPSLOCK},    S{SDL_SCANCODE_CAPSLOCK}},
		M{P{0x84}, K{SDLK_F7},          S{SDL_SCANCODE_F7}}, // SELECT
		M{P{0x85}, K{SDLK_PRINTSCREEN}, S{SDL_SCANCODE_PRINTSCREEN}}, // PRINT
		// no key on 0x86 ?
		M{P{0x87}, K{SDLK_RIGHT},       S{SDL_SCANCODE_RIGHT}},

		M{P{0x90}, K{SDLK_KP_0},        S{SDL_SCANCODE_KP_0}},
		M{P{0x91}, K{SDLK_KP_1},        S{SDL_SCANCODE_KP_1}},
		M{P{0x92}, K{SDLK_KP_2},        S{SDL_SCANCODE_KP_2}},
		M{P{0x93}, K{SDLK_KP_3},        S{SDL_SCANCODE_KP_3}},
		M{P{0x94}, K{SDLK_KP_4},        S{SDL_SCANCODE_KP_4}},
		M{P{0x95}, K{SDLK_KP_5},        S{SDL_SCANCODE_KP_5}},
		M{P{0x96}, K{SDLK_KP_6},        S{SDL_SCANCODE_KP_6}},
		M{P{0x97}, K{SDLK_KP_7},        S{SDL_SCANCODE_KP_7}},

		M{P{0xA0}, K{SDLK_KP_8},        S{SDL_SCANCODE_KP_8}},
		M{P{0xA1}, K{SDLK_KP_9},        S{SDL_SCANCODE_KP_9}},
		M{P{0xA2}, K{SDLK_KP_PLUS},     S{SDL_SCANCODE_KP_PLUS}},
		M{P{0xA3}, K{SDLK_KP_MINUS},    S{SDL_SCANCODE_KP_MINUS}},
		M{P{0xA4}, K{SDLK_KP_MULTIPLY}, S{SDL_SCANCODE_KP_MULTIPLY}},
		M{P{0xA5}, K{SDLK_KP_DIVIDE},   S{SDL_SCANCODE_KP_DIVIDE}},
		M{P{0xA6}, K{SDLK_KP_PERIOD},   S{SDL_SCANCODE_KP_PERIOD}},
		M{P{0xA7}, K{SDLK_KP_COMMA},    S{SDL_SCANCODE_KP_COMMA}},
	};
	return mapping;
}

static constexpr auto getCvJoyMapping()
{
	// ColecoVision Joystick "Matrix"
	//
	// The hardware consists of 2 controllers that each have 2 triggers
	// and a 12-key keypad. They're not actually connected in a matrix,
	// but a ghosting-free matrix is the easiest way to model it in openMSX.
	//
	// row/bit  7     6     5     4     3     2     1     0
	//       +-----+-----+-----+-----+-----+-----+-----+-----+
	//   0   |TRIGB|TRIGA|     |     |LEFT |DOWN |RIGHT| UP  |  controller 1
	//   1   |TRIGB|TRIGA|     |     |LEFT |DOWN |RIGHT| UP  |  controller 2
	//   2   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |  controller 1
	//   3   |     |     |     |     |  #  |  *  |  9  |  8  |  controller 1
	//   4   |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |  controller 2
	//   5   |     |     |     |     |  #  |  *  |  9  |  8  |  controller 2
	//       +-----+-----+-----+-----+-----+-----+-----+-----+
	using M = MsxKeyScanMapping;
	using P = KeyMatrixPosition;
	using K = std::array<SDL_Keycode, 3>;
	using S = std::array<SDL_Scancode, 3>;
	std::array mapping = {
		M{P{0x00}, K{SDLK_UP},      S{SDL_SCANCODE_UP}},
		M{P{0x01}, K{SDLK_RIGHT},   S{SDL_SCANCODE_RIGHT}},
		M{P{0x02}, K{SDLK_DOWN},    S{SDL_SCANCODE_DOWN}},
		M{P{0x03}, K{SDLK_LEFT},    S{SDL_SCANCODE_LEFT}},
		M{P{0x06}, K{SDLK_SPACE,        SDLK_RCTRL},
		           S{SDL_SCANCODE_SPACE, SDL_SCANCODE_RCTRL}},
		M{P{0x07}, K{SDLK_RSHIFT,         SDLK_RALT,         SDLK_LALT},
		           S{SDL_SCANCODE_RSHIFT, SDL_SCANCODE_RALT, SDL_SCANCODE_LALT}},

		M{P{0x10}, K{SDLK_w},       S{SDL_SCANCODE_W}},
		M{P{0x11}, K{SDLK_d},       S{SDL_SCANCODE_D}},
		M{P{0x12}, K{SDLK_s},       S{SDL_SCANCODE_S}},
		M{P{0x13}, K{SDLK_a},       S{SDL_SCANCODE_A}},
		M{P{0x16}, K{SDLK_LCTRL},   S{SDL_SCANCODE_LCTRL}},
		M{P{0x17}, K{SDLK_LSHIFT},  S{SDL_SCANCODE_LSHIFT}},

		M{P{0x20}, K{SDLK_0, SDLK_KP_0},       S{SDL_SCANCODE_0}},
		M{P{0x21}, K{SDLK_1, SDLK_KP_1},       S{SDL_SCANCODE_1}},
		M{P{0x22}, K{SDLK_2, SDLK_KP_2},       S{SDL_SCANCODE_2}},
		M{P{0x23}, K{SDLK_3, SDLK_KP_3},       S{SDL_SCANCODE_3}},
		M{P{0x24}, K{SDLK_4, SDLK_KP_4},       S{SDL_SCANCODE_4}},
		M{P{0x25}, K{SDLK_5, SDLK_KP_5},       S{SDL_SCANCODE_5}},
		M{P{0x26}, K{SDLK_6, SDLK_KP_6}, S{SDL_SCANCODE_6}},
		M{P{0x27}, K{SDLK_7, SDLK_KP_7}, S{SDL_SCANCODE_7}},

		M{P{0x30}, K{SDLK_8, SDLK_KP_8}, S{SDL_SCANCODE_8}},
		M{P{0x31}, K{SDLK_9, SDLK_KP_9}, S{SDL_SCANCODE_9}},
		M{P{0x32}, K{SDLK_MINUS,         SDLK_KP_MULTIPLY,         SDLK_KP_MINUS},
		           S{SDL_SCANCODE_MINUS, SDL_SCANCODE_KP_MULTIPLY, SDL_SCANCODE_KP_MINUS}}, // *
		M{P{0x33}, K{SDLK_EQUALS,         SDLK_KP_DIVIDE,         SDLK_KP_PLUS},
		           S{SDL_SCANCODE_EQUALS, SDL_SCANCODE_KP_DIVIDE, SDL_SCANCODE_KP_PLUS}},// #

		M{P{0x40}, K{SDLK_u},       S{SDL_SCANCODE_U}}, // 0
		M{P{0x41}, K{SDLK_v},       S{SDL_SCANCODE_V}}, // 1
		M{P{0x42}, K{SDLK_b},       S{SDL_SCANCODE_B}}, // 2
		M{P{0x43}, K{SDLK_n},       S{SDL_SCANCODE_N}}, // 3
		M{P{0x44}, K{SDLK_f},       S{SDL_SCANCODE_F}}, // 4
		M{P{0x45}, K{SDLK_g},       S{SDL_SCANCODE_G}}, // 5
		M{P{0x46}, K{SDLK_h},       S{SDL_SCANCODE_H}}, // 6
		M{P{0x47}, K{SDLK_r},       S{SDL_SCANCODE_R}}, // 7

		M{P{0x50}, K{SDLK_t},       S{SDL_SCANCODE_T}}, // 8
		M{P{0x51}, K{SDLK_y},       S{SDL_SCANCODE_Y}}, // 9
		M{P{0x52}, K{SDLK_j},       S{SDL_SCANCODE_J}}, // *
		M{P{0x53}, K{SDLK_m},       S{SDL_SCANCODE_M}}, // #
	};
	return mapping;
}

static constexpr auto getSegaMapping()
{
	// Sega SC-3000 / SK-1100 Keyboard Matrix
	//
	// row/bit  7     6     5     4     3     2     1     0
	//       +-----+-----+-----+-----+-----+-----+-----+-----+  PPI
	//   0   |  I  |  K  |  ,  | eng |  Z  |  A  |  Q  |  1  |  A0
	//   1   |  O  |  L  |  .  |space|  X  |  S  |  W  |  2  |  A1
	//   2   |  P  |  ;  |  /  |home |  C  |  D  |  E  |  3  |  A2
	//   3   |  @  |  :  | pi  |ins  |  V  |  F  |  R  |  4  |  A3
	//   4   |  [  |  ]  |down |     |  B  |  G  |  T  |  5  |  A4
	//   5   |     | cr  |left |     |  N  |  H  |  Y  |  6  |  A5
	//   6   |     | up  |right|     |  M  |  J  |  U  |  7  |  A6
	//       +-----+-----+-----+-----+-----+-----+-----+-----+
	//   7   |     |     |     |     |     |     |     |  8  |  B0
	//   8   |     |     |     |     |     |     |     |  9  |  B1
	//   9   |     |     |     |     |     |     |     |  0  |  B2
	//   A   |     |     |     |     |     |     |     |  -  |  B3
	//   B   |     |     |     |     |     |     |     |  ^  |  B4
	//   C   |     |     |     |     |func |     |     | cur |  B5
	//   D   |     |     |     |     |shift|ctrl |graph|break|  B6
	//       +-----+-----+-----+-----+-----+-----+-----+-----+
	// Issues:
	// - graph is a lock key and gets pressed when using alt-tab
	// - alt-F7 is bound to quick-load
	using M = MsxKeyScanMapping;
	using P = KeyMatrixPosition;
	using K = std::array<SDL_Keycode, 3>;
	using S = std::array<SDL_Scancode, 3>;
	std::array mapping = {
		M{P{0x00}, K{SDLK_1,         SDLK_KP_1},
		           S{SDL_SCANCODE_1, SDL_SCANCODE_KP_1}},
		M{P{0x01}, K{SDLK_q},           S{SDL_SCANCODE_Q}},
		M{P{0x02}, K{SDLK_a},           S{SDL_SCANCODE_A}},
		M{P{0x03}, K{SDLK_z},           S{SDL_SCANCODE_Z}},
		M{P{0x04}, K{SDLK_RALT},        S{SDL_SCANCODE_RALT}},  // eng
		M{P{0x05}, K{SDLK_COMMA},       S{SDL_SCANCODE_COMMA}},
		M{P{0x06}, K{SDLK_k},           S{SDL_SCANCODE_K}},
		M{P{0x07}, K{SDLK_i},           S{SDL_SCANCODE_I}},

		M{P{0x10}, K{SDLK_2,         SDLK_KP_2},
		          S{SDL_SCANCODE_2, SDL_SCANCODE_KP_2}},
		M{P{0x11}, K{SDLK_w},           S{SDL_SCANCODE_W}},
		M{P{0x12}, K{SDLK_s},           S{SDL_SCANCODE_S}},
		M{P{0x13}, K{SDLK_x},           S{SDL_SCANCODE_X}},
		M{P{0x14}, K{SDLK_SPACE},       S{SDL_SCANCODE_SPACE}},
		M{P{0x15}, K{SDLK_PERIOD,         SDLK_KP_PERIOD},
		           S{SDL_SCANCODE_PERIOD, SDL_SCANCODE_KP_PERIOD}},
		M{P{0x16}, K{SDLK_l},           S{SDL_SCANCODE_L}},
		M{P{0x17}, K{SDLK_o},           S{SDL_SCANCODE_O}},

		M{P{0x20}, K{SDLK_3,         SDLK_KP_3},
		           S{SDL_SCANCODE_3, SDL_SCANCODE_KP_3}},
		M{P{0x21}, K{SDLK_e},           S{SDL_SCANCODE_E}},
		M{P{0x22}, K{SDLK_d},           S{SDL_SCANCODE_D}},
		M{P{0x23}, K{SDLK_c},           S{SDL_SCANCODE_C}},
		M{P{0x24}, K{SDLK_HOME},        S{SDL_SCANCODE_HOME}},
		M{P{0x25}, K{SDLK_SLASH,         SDLK_KP_DIVIDE},
		           S{SDL_SCANCODE_SLASH, SDL_SCANCODE_KP_DIVIDE}},
		M{P{0x26}, K{SDLK_SEMICOLON,         SDLK_KP_PLUS},
		           S{SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_KP_PLUS}},
		M{P{0x27}, K{SDLK_p},           S{SDL_SCANCODE_P}},

		M{P{0x30}, K{SDLK_4,         SDLK_KP_4},
		           S{SDL_SCANCODE_4, SDL_SCANCODE_KP_4}},
		M{P{0x31}, K{SDLK_r},           S{SDL_SCANCODE_R}},
		M{P{0x32}, K{SDLK_f},           S{SDL_SCANCODE_F}},
		M{P{0x33}, K{SDLK_v},           S{SDL_SCANCODE_V}},
		M{P{0x34}, K{SDLK_INSERT},      S{SDL_SCANCODE_INSERT}},
		M{P{0x35}, K{SDLK_F7},          S{SDL_SCANCODE_F7}},         // pi
		M{P{0x36}, K{SDLK_QUOTE,              SDLK_KP_MULTIPLY},
		           S{SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_KP_MULTIPLY}}, // :
		M{P{0x37}, K{SDLK_BACKQUOTE},   S{SDL_SCANCODE_GRAVE}},      // @

		M{P{0x40}, K{SDLK_5,         SDLK_KP_5},
		           S{SDL_SCANCODE_5, SDL_SCANCODE_KP_5}},
		M{P{0x41}, K{SDLK_t},           S{SDL_SCANCODE_T}},
		M{P{0x42}, K{SDLK_g},           S{SDL_SCANCODE_G}},
		M{P{0x43}, K{SDLK_b},           S{SDL_SCANCODE_B}},
		// nothing on 0x44
		M{P{0x45}, K{SDLK_DOWN},        S{SDL_SCANCODE_DOWN}},
		M{P{0x46}, K{SDLK_RIGHTBRACKET},S{SDL_SCANCODE_RIGHTBRACKET}},
		M{P{0x47}, K{SDLK_LEFTBRACKET}, S{SDL_SCANCODE_LEFTBRACKET}},

		M{P{0x50}, K{SDLK_6,         SDLK_KP_6},
		           S{SDL_SCANCODE_6, SDL_SCANCODE_KP_6}},
		M{P{0x51}, K{SDLK_y},           S{SDL_SCANCODE_Y}},
		M{P{0x52}, K{SDLK_h},           S{SDL_SCANCODE_H}},
		M{P{0x53}, K{SDLK_n},           S{SDL_SCANCODE_N}},
		// nothing on 0x54
		M{P{0x55}, K{SDLK_LEFT},        S{SDL_SCANCODE_LEFT}},
		M{P{0x56}, K{SDLK_RETURN,         SDLK_KP_ENTER},
		           S{SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER}},
		//P{ not}hing on 0x57

		M{P{0x60}, K{SDLK_7,         SDLK_KP_7},
		           S{SDL_SCANCODE_7, SDL_SCANCODE_KP_7}},
		M{P{0x61}, K{SDLK_u},           S{SDL_SCANCODE_U}},
		M{P{0x62}, K{SDLK_j},           S{SDL_SCANCODE_J}},
		M{P{0x63}, K{SDLK_m},           S{SDL_SCANCODE_M}},
		// nothing on 0x64
		M{P{0x65}, K{SDLK_RIGHT},       S{SDL_SCANCODE_RIGHT}},
		M{P{0x66}, K{SDLK_UP},          S{SDL_SCANCODE_UP}},
		// nothing on 0x67

		M{P{0x70}, K{SDLK_8,         SDLK_KP_8},
		           S{SDL_SCANCODE_8, SDL_SCANCODE_KP_8}},
		M{P{0x80}, K{SDLK_9,         SDLK_KP_9},
		           S{SDL_SCANCODE_9, SDL_SCANCODE_KP_9}},
		M{P{0x90}, K{SDLK_0,         SDLK_KP_0},
		           S{SDL_SCANCODE_0, SDL_SCANCODE_KP_0}},
		M{P{0xA0}, K{SDLK_MINUS,         SDLK_KP_MINUS},
		           S{SDL_SCANCODE_MINUS, SDL_SCANCODE_KP_MINUS}},
		M{P{0xB0}, K{SDLK_EQUALS},      S{SDL_SCANCODE_EQUALS}}, // ^

		M{P{0xC0}, K{SDLK_BACKSLASH},   S{SDL_SCANCODE_BACKSLASH}}, // cur
		M{P{0xC3}, K{SDLK_TAB},         S{SDL_SCANCODE_TAB}},       // func

		M{P{0xD0}, K{SDLK_ESCAPE,         SDLK_F8},
		           S{SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F8}},   // break
		M{P{0xD1}, K{SDLK_LALT},        S{SDL_SCANCODE_LALT}}, // graph
		M{P{0xD2}, K{SDLK_LCTRL},       S{SDL_SCANCODE_LCTRL}},
		M{P{0xD3}, K{SDLK_LSHIFT,         SDLK_RSHIFT},
		        S{SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT}},
	};
	return mapping;
//// 0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
//   x  , x  , x  , x  , x  , x  , x  , x  ,0x34,0xC3, x  , x  , x  ,0x56, x  , x  , //000
//   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  ,0xD0, x  , x  , x  , x  , //010
//  0x14, x  , x  , x  , x  , x  , x  ,0x36, x  , x  , x  , x  ,0x05,0xA0,0x15,0x25, //020
//  0x90,0x00,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,0x36,0x26, x  ,0xB0, x  , x  , //030
//   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //040
//   x  ,0x55,0x66,0x65,0x45, x  , x  , x  , x  , x  , x  ,0x47,0xC0,0x46, x  , x  , //050
//  0x37,0x02,0x43,0x23,0x22,0x21,0x32,0x42,0x52,0x07,0x62,0x06,0x16,0x63,0x53,0x17, //060
//  0x27,0x01,0x31,0x12,0x41,0x61,0x33,0x11,0x13,0x51,0x03, x  , x  , x  , x  ,0x34, //070
//   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //080
//   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //090
//   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0A0
//   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0B0
//   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0C0
//   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0D0
//   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0E0
//   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //0F0
//  0x90,0x00,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,0x15,0x25,0x36,0xA0,0x26,0x56, //100
//   x  ,0x66,0x45,0x65,0x55,0x34,0x24, x  , x  , x  , x  , x  , x  , x  , x  , x  , //110
//  0x35,0xD0, x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  ,0xD3, //120
//  0xD3, x  ,0xD2,0x04,0xD1, x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //130
//   x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , x  , //140
}

static constexpr auto msxKeyCodeMapping    = extractKeyCodeMapping ([] { return getMSXMapping(); });
static constexpr auto msxScanCodeMapping   = extractScanCodeMapping([] { return getMSXMapping(); });
static constexpr auto sviKeyCodeMapping    = extractKeyCodeMapping ([] { return getSVIMapping(); });
static constexpr auto sviScanCodeMapping   = extractScanCodeMapping([] { return getSVIMapping(); });
static constexpr auto cvJoyKeyCodeMapping  = extractKeyCodeMapping ([] { return getCvJoyMapping(); });
static constexpr auto cvJoyScanCodeMapping = extractScanCodeMapping([] { return getCvJoyMapping(); });
static constexpr auto segaKeyCodeMapping   = extractKeyCodeMapping ([] { return getSegaMapping(); });
static constexpr auto segaScanCodeMapping  = extractScanCodeMapping([] { return getSegaMapping(); });

static constexpr array_with_enum_index<Keyboard::Matrix, std::span<const KeyCodeMsxMapping>> defaultKeyCodeMappings = {
	msxKeyCodeMapping,
	sviKeyCodeMapping,
	cvJoyKeyCodeMapping,
	segaKeyCodeMapping,
};
static constexpr array_with_enum_index<Keyboard::Matrix, std::span<const ScanCodeMsxMapping>> defaultScanCodeMappings = {
	msxScanCodeMapping,
	sviScanCodeMapping,
	cvJoyScanCodeMapping,
	segaScanCodeMapping,
};

Keyboard::Keyboard(MSXMotherBoard& motherBoard,
                   Scheduler& scheduler_,
                   CommandController& commandController_,
                   EventDistributor& eventDistributor,
                   MSXEventDistributor& msxEventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   Matrix matrix,
                   const DeviceConfig& config)
	: Schedulable(scheduler_)
	, commandController(commandController_)
	, msxEventDistributor(msxEventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, keyCodeTab (defaultKeyCodeMappings [matrix])
	, scanCodeTab(defaultScanCodeMappings[matrix])
	, modifierPos(modifierPosForMatrix[matrix])
	, keyMatrixUpCmd  (commandController, stateChangeDistributor, scheduler_)
	, keyMatrixDownCmd(commandController, stateChangeDistributor, scheduler_)
	, keyTypeCmd      (commandController, stateChangeDistributor, scheduler_)
	, msxcode2UnicodeCmd(commandController)
	, unicode2MsxcodeCmd(commandController)
	, capsLockAligner(eventDistributor, scheduler_)
	, keyboardSettings(commandController)
	, msxKeyEventQueue(scheduler_, commandController.getInterpreter())
	, keybDebuggable(motherBoard)
	, unicodeKeymap(config.getChildData(
		"keyboard_type", defaultKeymapForMatrix[matrix]))
	, hasKeypad(config.getChildDataAsBool("has_keypad", true))
	, blockRow11(matrix == Matrix::MSX
		&& !config.getChildDataAsBool("has_yesno_keys", false))
	, keyGhosting(config.getChildDataAsBool("key_ghosting", true))
	, keyGhostingSGCprotected(config.getChildDataAsBool(
		"key_ghosting_sgc_protected", true))
	, modifierIsLock(KeyInfo::CAPS_MASK
		| (config.getChildDataAsBool("code_kana_locks", false) ? KeyInfo::CODE_MASK : 0)
		| (config.getChildDataAsBool("graph_locks", false) ? KeyInfo::GRAPH_MASK : 0))
{
	std::ranges::fill(keyMatrix,     255);
	std::ranges::fill(cmdKeyMatrix,  255);
	std::ranges::fill(typeKeyMatrix, 255);
	std::ranges::fill(userKeyMatrix, 255);
	std::ranges::fill(hostKeyMatrix, 255);

	msxEventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
	// We do not listen for CONSOLE_OFF_EVENTS because rescanning the
	// keyboard can have unwanted side effects

	motherBoard.registerKeyboard(*this);
}

Keyboard::~Keyboard()
{
	auto& motherBoard = keybDebuggable.getMotherBoard();
	motherBoard.unregisterKeyboard(*this);

	stateChangeDistributor.unregisterListener(*this);
	msxEventDistributor.unregisterEventListener(*this);
}

const MsxChar2Unicode& Keyboard::getMsxChar2Unicode() const
{
	return unicodeKeymap.getMsxChars();
}

static constexpr void doKeyGhosting(std::span<uint8_t, KeyMatrixPosition::NUM_ROWS> matrix,
                                    bool protectRow6)
{
	// This routine enables key-ghosting as seen on a real MSX
	//
	// If on a real MSX in the keyboard matrix the
	// real buttons are pressed as in the left matrix
	//           then the matrix to the
	// 10111111  right will be read by   10110101
	// 11110101  because of the simple   10110101
	// 10111101  electrical connections  10110101
	//           that are established  by
	// the closed switches
	// However, some MSX models have protection against
	// key-ghosting for SHIFT, GRAPH and CODE keys
	// On those models, SHIFT, GRAPH and CODE are
	// connected to row 6 via a diode. It prevents that
	// SHIFT, GRAPH and CODE get ghosted to another
	// row.
	bool changedSomething = false;
	do {
		changedSomething = false;
		// TODO: On Sega keyboards, ghosting should probably be done separately
		//       for rows 0..6 and 7..14, since they're connected to different
		//       PPI ports.
		for (auto i : xrange(KeyMatrixPosition::NUM_ROWS - 1)) {
			auto row1 = matrix[i];
			for (auto j : xrange(i + 1, KeyMatrixPosition::NUM_ROWS)) {
				auto row2 = matrix[j];
				if ((row1 != row2) && ((row1 | row2) != 0xff)) {
					auto rowIold = matrix[i];
					auto rowJold = matrix[j];
					// TODO: The shift/graph/code key ghosting protection
					//       implementation is only correct for MSX.
					if (protectRow6 && i == 6) {
						matrix[i] = row1 & row2;
						matrix[j] = (row1 | 0x15) & row2;
						row1 &= row2;
					} else if (protectRow6 && j == 6) {
						matrix[i] = row1 & (row2 | 0x15);
						matrix[j] = row1 & row2;
						row1 &= (row2 | 0x15);
					} else {
						// not same and some common zero's
						//  --> inherit other zero's
						uint8_t newRow = row1 & row2;
						matrix[i] = newRow;
						matrix[j] = newRow;
						row1 = newRow;
					}
					if (rowIold != matrix[i] ||
					    rowJold != matrix[j]) {
						changedSomething = true;
					}
				}
			}
		}
	} while (changedSomething);
}

std::span<const uint8_t, KeyMatrixPosition::NUM_ROWS> Keyboard::getKeys() const
{
	if (keysChanged) {
		keysChanged = false;
		std::span matrix = keyTypeCmd.isActive() ? typeKeyMatrix : userKeyMatrix;
		for (auto row : xrange(KeyMatrixPosition::NUM_ROWS)) {
			keyMatrix[row] = cmdKeyMatrix[row] & matrix[row];
		}
		if (keyGhosting) {
			doKeyGhosting(keyMatrix, keyGhostingSGCprotected);
		}
	}
	return keyMatrix;
}

void Keyboard::transferHostKeyMatrix(const Keyboard& source)
{
	// This mechanism exists to solve the following problem:
	//   - play a game where the spacebar is constantly pressed (e.g.
	//     Road Fighter)
	//   - go back in time (press the reverse hotkey) while keeping the
	//     spacebar pressed
	//   - interrupt replay by pressing the cursor keys, still while
	//     keeping spacebar pressed
	// At the moment replay is interrupted, we need to resynchronize the
	// msx keyboard with the host keyboard. In the past we assumed the host
	// keyboard had no keys pressed. But this is wrong in the above
	// scenario. Now we remember the state of the host keyboard and
	// transfer that to the new keyboard(s) that get created for reverse.
	// When replay is stopped we restore this host keyboard state, see
	// stopReplay().

	for (auto row : xrange(KeyMatrixPosition::NUM_ROWS)) {
		hostKeyMatrix[row] = source.hostKeyMatrix[row];
	}
}

void Keyboard::setFocus(bool newFocus, EmuTime time)
{
	if (newFocus == focus) return;
	focus = newFocus;

	if (!stateChangeDistributor.isReplaying()) {
		syncHostKeyMatrix(time); // release all keys on lost focus
	}
}

/* Received an MSX event
 * Following events get processed:
 *  EventType::KEY_DOWN
 *  EventType::KEY_UP
 */
void Keyboard::signalMSXEvent(const Event& event,
                              EmuTime time) noexcept
{
	if (getType(event) == one_of(EventType::KEY_DOWN, EventType::KEY_UP)) {
		const auto& keyEvent = get_event<KeyEvent>(event);
		if (keyEvent.getRepeat()) return;
		// Ignore possible console on/off events:
		// we do not re-scan the keyboard since this may lead to
		// an unwanted pressing of <return> in MSX after typing
		// "set console off" in the console.
		msxKeyEventQueue.process_asap(time, event);
	}
}

void Keyboard::signalStateChange(const StateChange& event)
{
	const auto* kms = dynamic_cast<const KeyMatrixState*>(&event);
	if (!kms) return;

	userKeyMatrix[kms->getRow()] &= uint8_t(~kms->getPress());
	userKeyMatrix[kms->getRow()] |=          kms->getRelease();
	keysChanged = true; // do ghosting at next getKeys()
}

void Keyboard::stopReplay(EmuTime time) noexcept
{
	syncHostKeyMatrix(time);
}

void Keyboard::syncHostKeyMatrix(EmuTime time)
{
	for (auto [row, hkm] : enumerate(hostKeyMatrix)) {
		changeKeyMatrixEvent(time, uint8_t(row), hkm);
	}
	msxModifiers = 0xff;
	msxKeyEventQueue.clear();
	lastUnicodeForScancode.clear();
}

uint8_t Keyboard::needsLockToggle(const UnicodeKeymap::KeyInfo& keyInfo) const
{
	return modifierIsLock
	     & (locksOn ^ keyInfo.modMask)
	     & unicodeKeymap.getRelevantMods(keyInfo);
}

void Keyboard::pressKeyMatrixEvent(EmuTime time, KeyMatrixPosition pos)
{
	if (!pos.isValid()) {
		// No such key.
		return;
	}
	auto row = pos.getRow();
	auto press = pos.getMask();
	if (((hostKeyMatrix[row] & press) == 0) &&
	    ((userKeyMatrix[row] & press) == 0)) {
		// Won't have any effect, ignore.
		return;
	}
	changeKeyMatrixEvent(time, row, hostKeyMatrix[row] & ~press);
}

void Keyboard::releaseKeyMatrixEvent(EmuTime time, KeyMatrixPosition pos)
{
	if (!pos.isValid()) {
		// No such key.
		return;
	}
	auto row = pos.getRow();
	auto release = pos.getMask();
	if (((hostKeyMatrix[row] & release) == release) &&
	    ((userKeyMatrix[row] & release) == release)) {
		// Won't have any effect, ignore.
		// Test scenario: during replay, exit the openmsx console with
		// the 'toggle console' command. The 'enter,release' event will
		// end up here. But it shouldn't stop replay.
		return;
	}
	changeKeyMatrixEvent(time, row, hostKeyMatrix[row] | release);
}

void Keyboard::changeKeyMatrixEvent(EmuTime time, uint8_t row, uint8_t newValue)
{
	if (!focus) newValue = 0xff;

	// This method already updates hostKeyMatrix[],
	// userKeyMatrix[] will soon be updated via KeyMatrixState events.
	hostKeyMatrix[row] = newValue;

	uint8_t diff = userKeyMatrix[row] ^ newValue;
	if (diff == 0) return;
	uint8_t press   = userKeyMatrix[row] & diff;
	uint8_t release = newValue           & diff;
	stateChangeDistributor.distributeNew<KeyMatrixState>(
		time, row, press, release);
}

/*
 * @return True iff a release event for the CODE/KANA key must be scheduled.
 */
bool Keyboard::processQueuedEvent(const Event& event, EmuTime time)
{
	auto mode = keyboardSettings.getMappingMode();

	const auto& keyEvent = get_event<KeyEvent>(event);
	bool down = getType(event) == EventType::KEY_DOWN;
	auto key = keyEvent.getKey();

	if (down) {
		auto codepointToUtf8 = [](uint32_t cp) {
			std::array<char, 4> buffer;
			auto* start = buffer.data();
			auto* end = utf8::unchecked::append(cp, start);
			return std::string(start, end);
		};

		// TODO: refactor debug(...) method to expect a std::string and then adapt
		// all invocations of it to provide a properly formatted string, using the C++
		// features for it.
		// Once that is done, debug(...) can pass the c_str() version of that string
		// to ad_printf(...) so that I don't have to make an explicit ad_printf(...)
		// invocation for each debug(...) invocation
		debug("Key pressed, unicode: 0x%04x (%s), keyCode: 0x%05x (%s), scanCode: 0x%03x (%s), keyName: %s\n",
		      keyEvent.getUnicode(),
		      codepointToUtf8(keyEvent.getUnicode()).c_str(),
		      keyEvent.getKeyCode(),
		      SDL_GetKeyName(keyEvent.getKeyCode()),
		      keyEvent.getScanCode(),
		      SDL_GetScancodeName(keyEvent.getScanCode()),
		      key.toString().c_str());
	} else {
		debug("Key released, keyCode: 0x%05x (%s), scanCode: 0x%03x (%s), keyName: %s\n",
		      keyEvent.getKeyCode(),
		      SDL_GetKeyName(keyEvent.getKeyCode()),
		      keyEvent.getScanCode(),
		      SDL_GetScancodeName(keyEvent.getScanCode()),
		      key.toString().c_str());
	}

	// Process dead keys.
	if (mode == KeyboardSettings::MappingMode::CHARACTER) {
		for (auto n : xrange(3)) {
			if (key.sym.sym == keyboardSettings.getDeadKeyHostKey(n)) {
				UnicodeKeymap::KeyInfo deadKey = unicodeKeymap.getDeadKey(n);
				if (deadKey.isValid()) {
					updateKeyMatrix(time, down, deadKey.pos);
					return false;
				}
			}
		}
	}

	if (key.sym.sym == SDLK_CAPSLOCK) {
		processCapslockEvent(time, down);
		return false;
	} else if (key.sym.sym == keyboardSettings.getCodeKanaHostKey()) {
		processCodeKanaChange(time, down);
		return false;
	} else if (key.sym.sym == SDLK_LALT) {
		processGraphChange(time, down);
		return false;
	} else if (key.sym.sym == SDLK_KP_ENTER) {
		processKeypadEnterKey(time, down);
		return false;
	} else {
		// To work around a Japanese keyboard Kanji mode bug. (Multi-character
		// input makes a keydown event without keyrelease message.)
		if (keyEvent.getScanCode() == SDL_SCANCODE_UNKNOWN) {
			return false;
		}
		return processKeyEvent(time, down, keyEvent);
	}
}

/*
 * Process a change (up or down event) of the CODE/KANA key
 * It presses or releases the key in the MSX keyboard matrix
 * and changes the kana-lock state in case of a press
 */
void Keyboard::processCodeKanaChange(EmuTime time, bool down)
{
	if (down) {
		locksOn ^= KeyInfo::CODE_MASK;
	}
	updateKeyMatrix(time, down, modifierPos[KeyInfo::Modifier::CODE]);
}

/*
 * Process a change (up or down event) of the GRAPH key
 * It presses or releases the key in the MSX keyboard matrix
 * and changes the graph-lock state in case of a press
 */
void Keyboard::processGraphChange(EmuTime time, bool down)
{
	if (down) {
		locksOn ^= KeyInfo::GRAPH_MASK;
	}
	updateKeyMatrix(time, down, modifierPos[KeyInfo::Modifier::GRAPH]);
}

/*
 * Process a change (up or down event) of the CAPSLOCK key
 * It presses or releases the key in the MSX keyboard matrix
 * and changes the capslock state in case of a press
 */
void Keyboard::processCapslockEvent(EmuTime time, bool down)
{
	if (SANE_CAPSLOCK_BEHAVIOR) {
		debug("Changing CAPS lock state according to SDL request\n");
		if (down) {
			locksOn ^= KeyInfo::CAPS_MASK;
		}
		updateKeyMatrix(time, down, modifierPos[KeyInfo::Modifier::CAPS]);
	} else {
		debug("Pressing CAPS lock and scheduling a release\n");
		locksOn ^= KeyInfo::CAPS_MASK;
		updateKeyMatrix(time, true, modifierPos[KeyInfo::Modifier::CAPS]);
		setSyncPoint(time + EmuDuration::hz(10)); // 0.1s (in MSX time)
	}
}

void Keyboard::executeUntil(EmuTime time)
{
	debug("Releasing CAPS lock\n");
	updateKeyMatrix(time, false, modifierPos[KeyInfo::Modifier::CAPS]);
}

void Keyboard::processKeypadEnterKey(EmuTime time, bool down)
{
	if (!hasKeypad && !keyboardSettings.getAlwaysEnableKeypad()) {
		// User entered on host keypad but this MSX model does not have one
		// Ignore the keypress/release
		return;
	}
	processSdlKey(time,
	              SDLKey::create(keyboardSettings.getKpEnterMode() == KeyboardSettings::KpEnterMode::MSX_KP_COMMA
	                                 ? SDLK_KP_ENTER : SDLK_RETURN,
	                             down));
}

/*
 * Process an SDL key press/release event. It concerns a
 * special key (e.g. SHIFT, UP, DOWN, F1, F2, ...) that can not
 * be unambiguously derived from a unicode character;
 *  Map the SDL key to an equivalent MSX key press/release event
 */
void Keyboard::processSdlKey(EmuTime time, SDLKey key)
{
	auto process = [&](KeyMatrixPosition pos) {
		assert(pos.isValid());
		if (pos.getRow() == 11 && blockRow11) {
			// do not process row 11 if we have no Yes/No keys
			return;
		}
		updateKeyMatrix(time, key.down, pos);
	};

	if (keyboardSettings.getMappingMode() == KeyboardSettings::MappingMode::POSITIONAL) {
		if ((key.sym.sym == SDLK_RIGHTBRACKET) && (key.sym.scancode == SDL_SCANCODE_BACKSLASH))
		{
			// host: Japanese keyboard "]" -> position as -> US keyboard "GRAVE"
			if (const auto* mapping = binary_find(scanCodeTab, SDL_SCANCODE_GRAVE, {}, &ScanCodeMsxMapping::hostScanCode)) {
				process(mapping->msx);
			}
		} else if (const auto* mapping = binary_find(scanCodeTab, key.sym.scancode, {}, &ScanCodeMsxMapping::hostScanCode)) {
			process(mapping->msx);
		}
	} else {
		if (const auto* mapping = binary_find(keyCodeTab, key.sym.sym, {}, &KeyCodeMsxMapping::hostKeyCode)) {
			process(mapping->msx);
		}
	}
}

/*
 * Update the MSX keyboard matrix
 */
void Keyboard::updateKeyMatrix(EmuTime time, bool down, KeyMatrixPosition pos)
{
	if (!pos.isValid()) {
		// No such key.
		return;
	}
	if (down) {
		pressKeyMatrixEvent(time, pos);
		// Keep track of the MSX modifiers.
		// The MSX modifiers sometimes get overruled by the unicode character
		// processing, in which case the unicode processing must be able to
		// restore them to the real key-combinations pressed by the user.
		for (auto [i, mp] : enumerate(modifierPos)) {
			if (pos == mp) {
				msxModifiers &= uint8_t(~(1 << i));
			}
		}
	} else {
		releaseKeyMatrixEvent(time, pos);
		for (auto [i, mp] : enumerate(modifierPos)) {
			if (pos == mp) {
				msxModifiers |= 1 << i;
			}
		}
	}
}

/*
 * Process an SDL key event;
 *  Check if it is a special key, in which case it can be directly
 *  mapped to the MSX matrix.
 *  Otherwise, retrieve the unicode character value for the event
 *  and map the unicode character to the key-combination that must
 *  be pressed to generate the equivalent character on the MSX
 * @return True iff a release event for the CODE/KANA key must be scheduled.
 */
bool Keyboard::processKeyEvent(EmuTime time, bool down, const KeyEvent& keyEvent)
{
	auto mode = keyboardSettings.getMappingMode();

	auto keyCode = keyEvent.getKeyCode();
	auto scanCode = keyEvent.getScanCode();
	auto key = keyEvent.getKey();

	bool isOnKeypad =
		(SDLK_KP_1 <= keyCode && keyCode <= SDLK_KP_0) || // note order is 1-9,0
		(keyCode == one_of(SDLK_KP_PERIOD, SDLK_KP_DIVIDE, SDLK_KP_MULTIPLY,
		                   SDLK_KP_MINUS,  SDLK_KP_PLUS));

	if (isOnKeypad && !hasKeypad &&
	    !keyboardSettings.getAlwaysEnableKeypad()) {
		// User entered on host keypad but this MSX model does not have one
		// Ignore the keypress/release
		return false;
	}
#ifdef __APPLE__
	bool positional = mode == KeyboardSettings::MappingMode::POSITIONAL;
	if ((key.sym.mod & KMOD_GUI) &&
	    (( positional && (keyEvent.getScanCode() == SDL_SCANCODE_I)) ||
	     (!positional && (keyCode == SDLK_i)))) {
		// Apple keyboards don't have an Insert key, use Cmd+I as an alternative.
		keyCode = SDLK_INSERT;
		key.sym.sym = SDLK_INSERT;
		key.sym.scancode = SDL_SCANCODE_INSERT;
		key.sym.mod &= ~KMOD_GUI;
		key.sym.unused = 0; // unicode
	}
#endif

	if (down) {
		UnicodeKeymap::KeyInfo keyInfo;
		unsigned unicode;
		if (isOnKeypad ||
		    mode == one_of(KeyboardSettings::MappingMode::KEY,
		                   KeyboardSettings::MappingMode::POSITIONAL)) {
			// User entered a key on numeric keypad or the driver is in
			// KEY/POSITIONAL mapping mode.
			// First option (keypad) maps to same unicode as some other key
			// combinations (e.g. digit on main keyboard or TAB/DEL).
			// Use unicode to handle the more common combination and use direct
			// matrix to matrix mapping for the exceptional cases (keypad).
			unicode = 0;
		} else {
			unicode = keyEvent.getUnicode();
			if ((unicode < 0x20) || ((0x7F <= unicode) && (unicode < 0xA0))) {
				// Control character in C0 or C1 range.
				// Use SDL's interpretation instead.
				unicode = 0;
			} else if (utf8::is_pua(unicode)) {
				// Code point in Private Use Area: undefined by Unicode,
				// so we rely on SDL's interpretation instead.
				// For example the Mac's cursor keys are in this range.
				unicode = 0;
			} else {
				keyInfo = unicodeKeymap.get(unicode);
				if (!keyInfo.isValid()) {
					// Unicode does not exist in our mapping; try to process
					// the key using its keycode instead.
					unicode = 0;
				}
			}
		}

		// Remember which unicode character is currently derived
		// from this SDL key. It must be stored here (during key-press)
		// because during key-release SDL never returns the unicode
		// value (it always returns the value 0). But we must know
		// the unicode value in order to be able to perform the correct
		// key-combination-release in the MSX
		if (auto it = std::ranges::lower_bound(lastUnicodeForScancode, scanCode, {}, &std::pair<int32_t, uint32_t>::first);
		    (it != lastUnicodeForScancode.end()) && (it->first == scanCode)) {
			// after a while we can overwrite existing elements, and
			// then we stop growing/reallocating this vector
			it->second = unicode;
		} else {
			// insert new element (in the right location)
			lastUnicodeForScancode.emplace(it, scanCode, unicode);
		}

		if (unicode == 0) {
			// It was an ambiguous key (numeric key-pad, CTRL+character)
			// or a special key according to SDL (like HOME, INSERT, etc)
			// or a first keystroke of a composed key
			// (e.g. altr-gr + = on azerty keyboard) or driver is in
			// direct SDL mapping mode:
			// Perform direct SDL matrix to MSX matrix mapping
			// But only when it is not a first keystroke of a
			// composed key
			if (!(key.sym.mod & KMOD_MODE)) {
				processSdlKey(time, key);
			}
			return false;
		} else {
			// It is a unicode character; map it to the right key-combination
			return pressUnicodeByUser(time, keyInfo, unicode, true);
		}
	} else {
		// key was released
		auto it = std::ranges::lower_bound(lastUnicodeForScancode, scanCode, {}, &std::pair<int32_t, uint32_t>::first);
		unsigned unicode = ((it != lastUnicodeForScancode.end()) && (it->first == scanCode))
		                 ? it->second // get the unicode that was derived from this key
		                 : 0;
		if (unicode == 0) {
			// It was a special key, perform matrix to matrix mapping
			// But only when it is not a first keystroke of a
			// composed key
			if (!(key.sym.mod & KMOD_MODE)) {
				processSdlKey(time, key);
			}
		} else {
			// It was a unicode character; map it to the right key-combination
			pressUnicodeByUser(time, unicodeKeymap.get(unicode), unicode, false);
		}
		return false;
	}
}

void Keyboard::processCmd(Interpreter& interp, std::span<const TclObject> tokens, bool up)
{
	unsigned row  = tokens[1].getInt(interp);
	unsigned mask = tokens[2].getInt(interp);
	if (row >= KeyMatrixPosition::NUM_ROWS) {
		throw CommandException("Invalid row");
	}
	if (mask >= 256) {
		throw CommandException("Invalid mask");
	}
	if (up) {
		cmdKeyMatrix[row] |= narrow_cast<uint8_t>(mask);
	} else {
		cmdKeyMatrix[row] &= narrow_cast<uint8_t>(~mask);
	}
	keysChanged = true;
}

/*
 * This routine processes unicode characters. It maps a unicode character
 * to the correct key-combination on the MSX.
 *
 * There are a few caveats with respect to the MSX and Host modifier keys
 * that you must be aware about if you want to understand why the routine
 * works as it works.
 *
 * Row 6 of the MSX keyboard matrix contains the MSX modifier keys:
 *   CTRL, CODE, GRAPH and SHIFT
 *
 * The SHIFT key is also a modifier key on the host machine. However, the
 * SHIFT key behaviour can differ between HOST and MSX for all 'special'
 * characters (anything but A-Z).
 * For example, on AZERTY host keyboard, user presses SHIFT+& to make the '1'
 * On MSX QWERTY keyboard, the same key-combination leads to '!'.
 * So this routine must not only PRESS the SHIFT key when required according
 * to the unicode mapping table but it must also RELEASE the SHIFT key for all
 * these special keys when the user PRESSES the key/character.
 *
 * On the other hand, for A-Z, this routine must not touch the SHIFT key at all.
 * Otherwise it might give strange behaviour when CAPS lock is on (which also
 * acts as a key-modifier for A-Z). The routine can rely on the fact that
 * SHIFT+A-Z behaviour is the same on all host and MSX keyboards. It is
 * approximately the only part of keyboards that is de-facto standardized :-)
 *
 * For the other modifiers (CTRL, CODE and GRAPH), the routine must be able to
 * PRESS them when required but there is no need to RELEASE them during
 * character press. On the contrary; the host keys that map to CODE and GRAPH
 * do not work as modifiers on the host itself, so if the routine would release
 * them, it would give wrong result.
 * For example, 'ALT-A' on Host will lead to unicode character 'a', just like
 * only pressing the 'A' key. The MSX however must know about the difference.
 *
 * As a reminder: here is the build-up of row 6 of the MSX key matrix
 *              7    6     5     4    3      2     1    0
 * row  6   |  F3 |  F2 |  F1 | code| caps|graph| ctrl|shift|
 */
bool Keyboard::pressUnicodeByUser(
		EmuTime time, UnicodeKeymap::KeyInfo keyInfo, unsigned unicode,
		bool down)
{
	bool insertCodeKanaRelease = false;
	if (down) {
		if ((needsLockToggle(keyInfo) & KeyInfo::CODE_MASK) &&
				keyboardSettings.getAutoToggleCodeKanaLock()) {
			// Code Kana locks, is in wrong state and must be auto-toggled:
			// Toggle it by pressing the lock key and scheduling a
			// release event
			locksOn ^= KeyInfo::CODE_MASK;
			pressKeyMatrixEvent(time, modifierPos[KeyInfo::Modifier::CODE]);
			insertCodeKanaRelease = true;
		} else {
			// Press the character key and related modifiers
			// Ignore the CODE key in case that Code Kana locks
			// (e.g. do not press it).
			// Ignore the GRAPH key in case that Graph locks
			// Always ignore CAPSLOCK mask (assume that user will
			// use real CAPS lock to switch/ between hiragana and
			// katakana on japanese model)
			pressKeyMatrixEvent(time, keyInfo.pos);

			uint8_t modMask = keyInfo.modMask & ~modifierIsLock;
			if (('A' <= unicode && unicode <= 'Z') || ('a' <= unicode && unicode <= 'z')) {
				// For a-z and A-Z, leave SHIFT unchanged, this to cater
				// for difference in behaviour between host and emulated
				// machine with respect to how the combination of CAPSLOCK
				// and SHIFT is interpreted for these characters.
				modMask &= ~KeyInfo::SHIFT_MASK;
			} else {
				// Release SHIFT if our character does not require it.
				if (~modMask & KeyInfo::SHIFT_MASK) {
					releaseKeyMatrixEvent(time, modifierPos[KeyInfo::Modifier::SHIFT]);
				}
			}
			// Press required modifiers for our character.
			// Note that these modifiers are only pressed, never released.
			for (auto [i, mp] : enumerate(modifierPos)) {
				if ((modMask >> i) & 1) {
					pressKeyMatrixEvent(time, mp);
				}
			}
		}
	} else {
		releaseKeyMatrixEvent(time, keyInfo.pos);

		// Restore non-lock modifier keys.
		for (auto [i, mp] : enumerate(modifierPos)) {
			if (!((modifierIsLock >> i) & 1)) {
				// Do not simply unpress graph, ctrl, code and shift but
				// restore them to the values currently pressed by the user.
				if ((msxModifiers >> i) & 1) {
					releaseKeyMatrixEvent(time, mp);
				} else {
					pressKeyMatrixEvent(time, mp);
				}
			}
		}
	}
	keysChanged = true;
	return insertCodeKanaRelease;
}

/*
 * Press an ASCII character. It is used by the 'Insert characters'
 * function that is exposed to the console.
 * The characters are inserted in a separate keyboard matrix, to prevent
 * interference with the keypresses of the user on the MSX itself.
 *
 * @returns:
 *   zero  : handling this character is done
 * non-zero: typing this character needs additional actions
 *    bits 0-4: release these modifiers and call again
 *    bit   7 : simply call again (used by SHIFT+GRAPH heuristic)
 */
uint8_t Keyboard::pressAscii(unsigned unicode, bool down)
{
	uint8_t releaseMask = 0;
	UnicodeKeymap::KeyInfo keyInfo = unicodeKeymap.get(unicode);
	if (!keyInfo.isValid()) {
		return releaseMask;
	}
	uint8_t modMask = keyInfo.modMask & ~modifierIsLock;
	if (down) {
		// check for modifier toggles
		uint8_t toggleLocks = needsLockToggle(keyInfo);
		for (auto [i, mp] : enumerate(modifierPos)) {
			if ((toggleLocks >> i) & 1) {
				debug("Toggling lock %d\n", i);
				locksOn ^= 1 << i;
				releaseMask |= 1 << i;
				typeKeyMatrix[mp.getRow()] &= uint8_t(~mp.getMask());
			}
		}
		if (releaseMask == 0) {
			debug("Key pasted, unicode: 0x%04x, row: %02d, col: %d, modMask: %02x\n",
			      unicode, keyInfo.pos.getRow(), keyInfo.pos.getColumn(), modMask);
			// Workaround MSX-BIOS(?) bug/limitation:
			//
			// Under these conditions:
			// - Typing a graphical MSX character 00-1F (such a char 'x' gets
			//   printed as chr$(1) followed by chr$(x+64)).
			// - Typing this character requires pressing SHIFT+GRAPH and one
			//   'regular' key.
			// - And all 3 keys are immediately pressed simultaneously.
			// Then, from time to time, instead of the intended character 'x'
			// (00-1F), the wrong character 'x+64' gets printed.
			// When first SHIFT+GRAPH is pressed, and only one frame later the
			// other keys is pressed (additionally), this problem seems to
			// disappear.
			//
			// After implementing the above we found that a similar problem
			// occurs when:
			// - a GRAPH + <x> (without SHIFT) key combo is typed
			// - immediately after a key combo with GRAPH + SHIFT + <x>.
			// For example:
			//   type "\u2666\u266a"
			// from time to time 2nd character got wrongly typed as a
			// 'M' (instead of a musical note symbol). But typing a sequence
			// of \u266a chars without a preceding \u2666 just works.
			//
			// To fix both these problems (and possibly still undiscovered
			// variations), I'm now extending the workaround to all characters
			// that are typed via a key combination that includes GRAPH.
			if (modMask & KeyInfo::GRAPH_MASK) {
				auto isPressed = [&](auto& key) {
					return (typeKeyMatrix[key.getRow()] & key.getMask()) == 0;
				};
				if (!isPressed(modifierPos[KeyInfo::Modifier::GRAPH])) {
					// GRAPH not yet pressed ->
					//  first press it before adding the non-modifier key
					releaseMask = TRY_AGAIN;
				}
			}
			// press modifiers
			for (auto [i, mp] : enumerate(modifierPos)) {
				if ((modMask >> i) & 1) {
					typeKeyMatrix[mp.getRow()] &= uint8_t(~mp.getMask());
				}
			}
			if (releaseMask == 0) {
				// press key
				typeKeyMatrix[keyInfo.pos.getRow()] &= uint8_t(~keyInfo.pos.getMask());
			}
		}
	} else {
		typeKeyMatrix[keyInfo.pos.getRow()] |= keyInfo.pos.getMask();
		for (auto [i, mp] : enumerate(modifierPos)) {
			if ((modMask >> i) & 1) {
				typeKeyMatrix[mp.getRow()] |= mp.getMask();
			}
		}
	}
	keysChanged = true;
	return releaseMask;
}

/*
 * Press a lock key. It is used by the 'Insert characters'
 * function that is exposed to the console.
 * The characters are inserted in a separate keyboard matrix, to prevent
 * interference with the keypresses of the user on the MSX itself
 */
void Keyboard::pressLockKeys(uint8_t lockKeysMask, bool down)
{
	for (auto [i, mp] : enumerate(modifierPos)) {
		if ((lockKeysMask >> i) & 1) {
			if (down) {
				// press lock key
				typeKeyMatrix[mp.getRow()] &= uint8_t(~mp.getMask());
			} else {
				// release lock key
				typeKeyMatrix[mp.getRow()] |= mp.getMask();
			}
		}
	}
	keysChanged = true;
}

/*
 * Check if there are common keys in the MSX matrix for
 * two different unicodes.
 * It is used by the 'insert keys' function to determine if it has to wait for
 * a short while after releasing a key (to enter a certain character) before
 * pressing the next key (to enter the next character)
 */
bool Keyboard::commonKeys(unsigned unicode1, unsigned unicode2) const
{
	// get row / mask of key (note: ignore modifier mask)
	auto keyPos1 = unicodeKeymap.get(unicode1).pos;
	auto keyPos2 = unicodeKeymap.get(unicode2).pos;

	return keyPos1 == keyPos2 && keyPos1.isValid();
}

void Keyboard::debug(const char* format, ...) const
{
	if (keyboardSettings.getTraceKeyPresses()) {
		va_list args;
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
	}
}


// class KeyMatrixUpCmd

Keyboard::KeyMatrixUpCmd::KeyMatrixUpCmd(
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
		scheduler_, "keymatrixup")
{
}

void Keyboard::KeyMatrixUpCmd::execute(
	std::span<const TclObject> tokens, TclObject& /*result*/, EmuTime /*time*/)
{
	checkNumArgs(tokens, 3, Prefix{1}, "row mask");
	auto& keyboard = OUTER(Keyboard, keyMatrixUpCmd);
	keyboard.processCmd(getInterpreter(), tokens, true);
}

std::string Keyboard::KeyMatrixUpCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "keymatrixup <row> <bitmask>  release a key in the keyboard matrix\n";
}


// class KeyMatrixDownCmd

Keyboard::KeyMatrixDownCmd::KeyMatrixDownCmd(CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
		scheduler_, "keymatrixdown")
{
}

void Keyboard::KeyMatrixDownCmd::execute(std::span<const TclObject> tokens,
                                         TclObject& /*result*/, EmuTime /*time*/)
{
	checkNumArgs(tokens, 3, Prefix{1}, "row mask");
	auto& keyboard = OUTER(Keyboard, keyMatrixDownCmd);
	keyboard.processCmd(getInterpreter(), tokens, false);
}

std::string Keyboard::KeyMatrixDownCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "keymatrixdown <row> <bitmask>  press a key in the keyboard matrix\n";
}


// class MsxKeyEventQueue

Keyboard::MsxKeyEventQueue::MsxKeyEventQueue(
		Scheduler& scheduler_, Interpreter& interp_)
	: Schedulable(scheduler_)
	, interp(interp_)
{
}

void Keyboard::MsxKeyEventQueue::process_asap(
	EmuTime time, const Event& event)
{
	bool processImmediately = eventQueue.empty();
	eventQueue.push_back(event);
	if (processImmediately) {
		executeUntil(time);
	}
}

void Keyboard::MsxKeyEventQueue::clear()
{
	eventQueue.clear();
	removeSyncPoint();
}

void Keyboard::MsxKeyEventQueue::executeUntil(EmuTime time)
{
	// Get oldest event from the queue and process it
	const Event& event = eventQueue.front();
	auto& keyboard = OUTER(Keyboard, msxKeyEventQueue);
	bool insertCodeKanaRelease = keyboard.processQueuedEvent(event, time);

	if (insertCodeKanaRelease) {
		// The processor pressed the CODE/KANA key
		// Schedule a CODE/KANA release event, to be processed
		// before any of the other events in the queue
		eventQueue.emplace_front(KeyUpEvent::create(keyboard.keyboardSettings.getCodeKanaHostKey()));
	} else {
		// The event has been completely processed. Delete it from the queue
		if (!eventQueue.empty()) {
			eventQueue.pop_front();
		} else {
			// it's possible clear() has been called
			// (indirectly from keyboard.processQueuedEvent())
		}
	}

	if (!eventQueue.empty()) {
		// There are still events. Process them in 1/15s from now
		setSyncPoint(time + EmuDuration::hz(15));
	}
}


// class KeyInserter

Keyboard::KeyInserter::KeyInserter(
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
		scheduler_, "type_via_keyboard")
	, Schedulable(scheduler_)
{
}

void Keyboard::KeyInserter::execute(
	std::span<const TclObject> tokens, TclObject& /*result*/, EmuTime /*time*/)
{
	checkNumArgs(tokens, AtLeast{2}, "?-release? ?-freq hz? ?-cancel? text");

	bool cancel = false;
	releaseBeforePress = false;
	typingFrequency = 15;
	std::array info = {
		flagArg("-cancel", cancel),
		flagArg("-release", releaseBeforePress),
		valueArg("-freq", typingFrequency),
	};
	auto arguments = parseTclArgs(getInterpreter(), tokens.subspan(1), info);

	if (typingFrequency <= 0) {
		throw CommandException("Wrong argument for -freq (should be a positive number)");
	}
	if (cancel) {
		text_utf8.clear();
		return;
	}

	if (arguments.size() != 1) throw SyntaxError();

	type(arguments[0].getString());
}

std::string Keyboard::KeyInserter::help(std::span<const TclObject> /*tokens*/) const
{
	return "Type a string in the emulated MSX.\n"
	       "Use -release to make sure the keys are always released before typing new ones (necessary for some game input routines, but in general, this means typing is twice as slow).\n"
	       "Use -freq to tweak how fast typing goes and how long the keys will be pressed (and released in case -release was used). Keys will be typed at the given frequency and will remain pressed/released for 1/freq seconds\n"
	       "Use -cancel to cancel a (long) in-progress type command.";
}

void Keyboard::KeyInserter::tabCompletion(std::vector<std::string>& tokens) const
{
	using namespace std::literals;
	static constexpr std::array options = {"-release"sv, "-freq"sv};
	completeString(commandController, tokens, options);
}

void Keyboard::KeyInserter::type(std::string_view str)
{
	if (str.empty()) {
		return;
	}
	const auto& keyboard = OUTER(Keyboard, keyTypeCmd);
	oldLocksOn = keyboard.locksOn;
	if (text_utf8.empty()) {
		reschedule(getCurrentTime());
	}
	text_utf8.append(str.data(), str.size());
}

void Keyboard::KeyInserter::executeUntil(EmuTime time)
{
	auto& keyboard = OUTER(Keyboard, keyTypeCmd);
	if (lockKeysMask != 0) {
		// release CAPS and/or Code/Kana Lock keys
		keyboard.pressLockKeys(lockKeysMask, false);
	}
	if (releaseLast) {
		keyboard.pressAscii(last, false); // release previous character
	}
	if (text_utf8.empty()) {
		releaseLast = false;
		keyboard.debug("Restoring locks: %02X -> %02X\n", keyboard.locksOn, oldLocksOn);
		uint8_t diff = oldLocksOn ^ keyboard.locksOn;
		lockKeysMask = diff;
		if (diff != 0) {
			// press CAPS, GRAPH and/or Code/Kana Lock keys
			keyboard.locksOn ^= diff;
			keyboard.pressLockKeys(diff, true);
			reschedule(time);
		}
		return;
	}

	try {
		auto it = begin(text_utf8);
		unsigned current = utf8::next(it, end(text_utf8));
		if (releaseLast && (releaseBeforePress || keyboard.commonKeys(last, current))) {
			// There are common keys between previous and current character
			// Do not immediately press again but give MSX the time to notice
			// that the keys have been released
			releaseLast = false;
		} else {
			// All keys in current char differ from previous char. The new keys
			// can immediately be pressed
			lockKeysMask = keyboard.pressAscii(current, true);
			if (lockKeysMask == 0) {
				last = current;
				releaseLast = true;
				text_utf8.erase(begin(text_utf8), it);
			} else if (lockKeysMask & TRY_AGAIN) {
				lockKeysMask &= ~TRY_AGAIN;
				releaseLast = false;
			} else if (releaseBeforePress) {
				releaseLast = true;
			}
		}
		reschedule(time);
	} catch (std::exception&) {
		// utf8 encoding error
		text_utf8.clear();
	}
}

void Keyboard::KeyInserter::reschedule(EmuTime time)
{
	setSyncPoint(time + EmuDuration::hz(typingFrequency));
}


// Commands for conversion between msxcode <-> unicode.

Keyboard::Msxcode2UnicodeCmd::Msxcode2UnicodeCmd(CommandController& commandController_)
	: Command(commandController_, "msxcode2unicode")
{
}

void Keyboard::Msxcode2UnicodeCmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, Between{2, 3}, "msx-string ?fallback?");

	auto& interp = getInterpreter();
	const auto& keyboard = OUTER(Keyboard, msxcode2UnicodeCmd);
	const auto& msxChars = keyboard.unicodeKeymap.getMsxChars();

	auto msx = tokens[1].getBinary();
	auto fallback = [&] -> std::function<uint32_t(uint8_t)> {
		if (tokens.size() < 3) {
			// If no fallback is given use space as replacement character
			return [](uint8_t) { return uint32_t(' '); };
		} else if (auto i = tokens[2].getOptionalInt()) {
			// If an integer is given use that as a unicode character number
			return [i = *i](uint8_t) { return uint32_t(i); };
		} else {
			// Otherwise use the given string as the name of a Tcl proc,
			// That proc is (possibly later) invoked with a msx-character as input,
			// and it should return the replacement unicode character number.
			return [&](uint8_t m) {
				TclObject cmd{TclObject::MakeListTag{}, tokens[2], m};
				return uint32_t(cmd.executeCommand(interp).getInt(interp));
			};
		}
	}();

	result = msxChars.msxToUtf8(msx, fallback);
}

std::string Keyboard::Msxcode2UnicodeCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "msxcode2unicode <msx-string> [<fallback>]\n"
	       "returns a unicode string converted from an MSX-string, i.e. a string based on\n"
	       "MSX character codes.\n"
	       "The optional fallback used for each character that cannot be mapped for the\n"
	       "current MSX model can be:\n"
	       "- omitted: then space will be used as fallback character.\n"
	       "- an integer number: then this number will be used as unicode point to be the\n"
	       "  the fallback character.\n"
	       "- a Tcl proc, which expects one input character and must return one unicode\n"
	       "  point.";
}


Keyboard::Unicode2MsxcodeCmd::Unicode2MsxcodeCmd(CommandController& commandController_)
	: Command(commandController_, "unicode2msxcode")
{
}

void Keyboard::Unicode2MsxcodeCmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, Between{2, 3}, "unicode-string ?fallback?");

	auto& interp = getInterpreter();
	const auto& keyboard = OUTER(Keyboard, unicode2MsxcodeCmd);
	const auto& msxChars = keyboard.unicodeKeymap.getMsxChars();

	const auto& unicode = tokens[1].getString();
	auto fallback = [&] -> std::function<uint8_t(uint32_t)> {
		if (tokens.size() < 3) {
			// If no fallback is given use space as replacement character
			return [](uint32_t) { return uint8_t(' '); };
		} else if (auto i = tokens[2].getOptionalInt()) {
			// If an integer is given use that as a MSX character number
			return [i = *i](uint32_t) { return uint8_t(i); };
		} else {
			// Otherwise use the given string as the name of a Tcl proc,
			// That proc is (possibly later) invoked with a unicode character as
			// input, and it should return the replacement MSX character number.
			return [&](uint32_t u) {
				TclObject cmd{TclObject::MakeListTag{}, tokens[2], u};
				return uint8_t(cmd.executeCommand(interp).getInt(interp));
			};
		}
	}();

	result = msxChars.utf8ToMsx(unicode, fallback);
}

std::string Keyboard::Unicode2MsxcodeCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "unicode2msxcode <unicode-string> [<fallback>]\n"
	       "returns an MSX string, i.e. a string based on MSX character codes, converted\n"
	       "from a unicode string.\n"
	       "The optional fallback used for each character that cannot be mapped for the\n"
	       "current MSX model can be:\n"
	       "- omitted: then space will be used as fallback character.\n"
	       "- an integer number: then this number will be used as MSX character number to\n"
	       "  to be the fallback character.\n"
	       "- a Tcl proc, which expects one input character and must return one MSX\n"
	       "  character number.";
}


/*
 * class CapsLockAligner
 *
 * It is used to align MSX CAPS lock status with the host CAPS lock status
 * during the reset of the MSX or after the openMSX window regains focus.
 *
 * It listens to the 'BOOT' event and schedules the real alignment
 * 2 seconds later. Reason is that it takes a while before the MSX
 * reset routine starts monitoring the MSX keyboard.
 *
 * For focus regain, the alignment is done immediately.
 */
Keyboard::CapsLockAligner::CapsLockAligner(
		EventDistributor& eventDistributor_,
		Scheduler& scheduler_)
	: Schedulable(scheduler_)
	, eventDistributor(eventDistributor_)
{
	for (auto type : {EventType::BOOT, EventType::WINDOW}) {
		eventDistributor.registerEventListener(type,  *this);
	}
}

Keyboard::CapsLockAligner::~CapsLockAligner()
{
	for (auto type : {EventType::WINDOW, EventType::BOOT}) {
		eventDistributor.unregisterEventListener(type,  *this);
	}
}

bool Keyboard::CapsLockAligner::signalEvent(const Event& event)
{
	if constexpr (!SANE_CAPSLOCK_BEHAVIOR) {
		// don't even try
		return false;
	}

	if (state == IDLE) {
		EmuTime time = getCurrentTime();
		std::visit(overloaded{
			[&](const WindowEvent& e) {
				if (e.isMainWindow()) {
					const auto& evt = e.getSdlWindowEvent();
					if (evt.event == one_of(SDL_WINDOWEVENT_FOCUS_GAINED, SDL_WINDOWEVENT_FOCUS_LOST)) {
						alignCapsLock(time);
					}
				}
			},
			[&](const BootEvent&) {
				state = MUST_ALIGN_CAPSLOCK;
				setSyncPoint(time + EmuDuration::sec(2)); // 2s (MSX time)
			},
			[](const EventBase&) {
				// correct but causes excessive clang compile-time
				// UNREACHABLE;
			}
		}, event);
	}
	return false;
}

void Keyboard::CapsLockAligner::executeUntil(EmuTime time)
{
	switch (state) {
		case MUST_ALIGN_CAPSLOCK:
			alignCapsLock(time);
			break;
		case MUST_DISTRIBUTE_KEY_RELEASE: {
			auto& keyboard = OUTER(Keyboard, capsLockAligner);
			auto event = KeyUpEvent::create(SDLK_CAPSLOCK);
			keyboard.msxEventDistributor.distributeEvent(event, time);
			state = IDLE;
			break;
		}
		default:
			UNREACHABLE;
	}
}

/*
 * Align MSX caps lock state with host caps lock state
 * WARNING: This function assumes that the MSX will see and
 *          process the caps lock key press.
 *          If MSX misses the key press for whatever reason (e.g.
 *          interrupts are disabled), the caps lock state in this
 *          module will mismatch with the real MSX caps lock state
 * TODO: Find a solution for the above problem. For example by monitoring
 *       the MSX caps-lock LED state.
 */
void Keyboard::CapsLockAligner::alignCapsLock(EmuTime time)
{
	bool hostCapsLockOn = ((SDL_GetModState() & KMOD_CAPS) != 0);
	auto& keyboard = OUTER(Keyboard, capsLockAligner);
	if (bool(keyboard.locksOn & KeyInfo::CAPS_MASK) != hostCapsLockOn) {
		keyboard.debug("Resyncing host and MSX CAPS lock\n");
		// note: send out another event iso directly calling
		// processCapslockEvent() because we want this to be recorded
		auto event = KeyDownEvent::create(SDLK_CAPSLOCK);
		keyboard.msxEventDistributor.distributeEvent(event, time);
		keyboard.debug("Sending fake CAPS release\n");
		state = MUST_DISTRIBUTE_KEY_RELEASE;
		setSyncPoint(time + EmuDuration::hz(10)); // 0.1s (MSX time)
	} else {
		state = IDLE;
	}
}


// class KeybDebuggable

Keyboard::KeybDebuggable::KeybDebuggable(MSXMotherBoard& motherBoard_)
	: SimpleDebuggable(motherBoard_, "keymatrix", "MSX Keyboard Matrix",
	                   KeyMatrixPosition::NUM_ROWS)
{
}

uint8_t Keyboard::KeybDebuggable::read(unsigned address)
{
	const auto& keyboard = OUTER(Keyboard, keybDebuggable);
	return keyboard.getKeys()[address];
}

void Keyboard::KeybDebuggable::readBlock(unsigned start, std::span<uint8_t> output)
{
	const auto& keyboard = OUTER(Keyboard, keybDebuggable);
	copy_to_range(keyboard.getKeys().subspan(start, output.size()), output);
}

void Keyboard::KeybDebuggable::write(unsigned /*address*/, uint8_t /*value*/)
{
	// ignore
}


template<typename Archive>
void Keyboard::KeyInserter::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);
	ar.serialize("text", text_utf8,
	             "last", last,
	             "lockKeysMask", lockKeysMask,
	             "releaseLast", releaseLast);

	bool oldCodeKanaLockOn, oldGraphLockOn, oldCapsLockOn;
	if constexpr (!Archive::IS_LOADER) {
		oldCodeKanaLockOn = oldLocksOn & KeyInfo::CODE_MASK;
		oldGraphLockOn = oldLocksOn & KeyInfo::GRAPH_MASK;
		oldCapsLockOn = oldLocksOn & KeyInfo::CAPS_MASK;
	}
	ar.serialize("oldCodeKanaLockOn", oldCodeKanaLockOn,
	             "oldGraphLockOn",    oldGraphLockOn,
	             "oldCapsLockOn",     oldCapsLockOn);
	if constexpr (Archive::IS_LOADER) {
		oldLocksOn = (oldCodeKanaLockOn ? KeyInfo::CODE_MASK : 0)
		           | (oldGraphLockOn ? KeyInfo::GRAPH_MASK : 0)
		           | (oldCapsLockOn ? KeyInfo::CAPS_MASK : 0);
	}
}

// version 1: Initial version: {userKeyMatrix, dynKeymap, msxModifiers,
//            msxKeyEventQueue} was intentionally not serialized. The reason
//            was that after a loadstate, you want the MSX keyboard to reflect
//            the state of the host keyboard. So any pressed MSX keys from the
//            time the savestate was created are cleared.
// version 2: For reverse-replay it is important that snapshots contain the
//            full state of the MSX keyboard, so now we do serialize it.
// version 3: split cmdKeyMatrix into cmdKeyMatrix + typeKeyMatrix
// version 4: changed 'dynKeymap' to 'lastUnicodeForKeycode'
// version 5: changed 'lastUnicodeForKeycode' to 'lastUnicodeForScancode'
// TODO Is the assumption in version 1 correct (clear keyb state on load)?
//      If it is still useful for 'regular' loadstate, then we could implement
//      it by explicitly clearing the keyb state from the actual loadstate
//      command. (But let's only do this when experience shows it's really
//      better).
template<typename Archive>
void Keyboard::serialize(Archive& ar, unsigned version)
{
	ar.serialize("keyTypeCmd", keyTypeCmd,
	             "cmdKeyMatrix", cmdKeyMatrix);
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("typeKeyMatrix", typeKeyMatrix);
	} else {
		typeKeyMatrix = cmdKeyMatrix;
	}

	bool msxCapsLockOn, msxCodeKanaLockOn, msxGraphLockOn;
	if constexpr (!Archive::IS_LOADER) {
		msxCapsLockOn = locksOn & KeyInfo::CAPS_MASK;
		msxCodeKanaLockOn = locksOn & KeyInfo::CODE_MASK;
		msxGraphLockOn = locksOn & KeyInfo::GRAPH_MASK;
	}
	ar.serialize("msxCapsLockOn",     msxCapsLockOn,
	             "msxCodeKanaLockOn", msxCodeKanaLockOn,
	             "msxGraphLockOn",    msxGraphLockOn);
	if constexpr (Archive::IS_LOADER) {
		locksOn = (msxCapsLockOn ? KeyInfo::CAPS_MASK : 0)
		        | (msxCodeKanaLockOn ? KeyInfo::CODE_MASK : 0)
		        | (msxGraphLockOn ? KeyInfo::GRAPH_MASK : 0);
	}

	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("userKeyMatrix",    userKeyMatrix,
		             "msxmodifiers",     msxModifiers,
		             "msxKeyEventQueue", msxKeyEventQueue);
	}
	if (ar.versionAtLeast(version, 5)) {
		ar.serialize("lastUnicodeForScancode", lastUnicodeForScancode);
	}
	else if (ar.versionAtLeast(version, 4)) {
		// convert keycode to scancode
		std::vector<std::pair<SDL_Keycode, uint32_t> > lastUnicodeForKeycode;
		ar.serialize("lastUnicodeForKeycode", lastUnicodeForKeycode);
		lastUnicodeForScancode.clear();
		for (const auto& [keyCode, unicode] : lastUnicodeForKeycode) {
			auto scanCode = SDL_GetScancodeFromKey(keyCode);
			lastUnicodeForScancode.emplace_back(scanCode, unicode);
		}
	} else {
		// We can't (easily) reconstruct 'lastUnicodeForKeycode' from
		// 'dynKeymap'. Usually this won't cause problems. E.g.
		// typically you aren't typing at the same time that you create
		// a savestate. For replays it might matter, though usually
		// replays are about games, and typing in games is rare (for
		// cursors and space this isn't needed).
		//
		// So, at least for now, it's fine to not reconstruct this data.
	}
	// don't serialize hostKeyMatrix

	if constexpr (Archive::IS_LOADER) {
		// force recalculation of keyMatrix
		keysChanged = true;
	}
}
INSTANTIATE_SERIALIZE_METHODS(Keyboard);

template<typename Archive>
void Keyboard::MsxKeyEventQueue::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);

	// serialization of deque<Event> is not directly
	// supported by the serialization framework (main problem is the
	// constness, collections of shared_ptr to polymorphic objects are
	// not a problem). Worked around this by serializing the events in
	// ascii format. (In all practical cases this queue will anyway be
	// empty or contain very few elements).
	//ar.serialize("eventQueue", eventQueue);
	std::vector<std::string> eventStrs;
	if constexpr (!Archive::IS_LOADER) {
		eventStrs = to_vector(std::views::transform(
			eventQueue, [](const auto& e) { return toString(e); }));
	}
	ar.serialize("eventQueue", eventStrs);
	if constexpr (Archive::IS_LOADER) {
		assert(eventQueue.empty());
		for (const auto& s : eventStrs) {
			eventQueue.push_back(
				InputEventFactory::createInputEvent(s, interp));
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Keyboard::MsxKeyEventQueue);

} // namespace openmsx
