#ifndef KEYMAPPINGS_HH
#define KEYMAPPINGS_HH

#include <imgui.h>
#include <SDL.h>

#include <algorithm>
#include <array>

namespace openmsx {

// Here we define the mapping between ImGui and SDL (just once).
// This will be used to generate two mapping functions (for both directions).
struct ImGui_SDL_Key {
	ImGuiKey imgui;
	SDL_Keycode sdl;
};
inline constexpr auto unsortedKeys = std::to_array<ImGui_SDL_Key>({
	{ImGuiKey_Tab,            SDLK_TAB},
	{ImGuiKey_LeftArrow,      SDLK_LEFT},
	{ImGuiKey_RightArrow,     SDLK_RIGHT},
	{ImGuiKey_UpArrow,        SDLK_UP},
	{ImGuiKey_DownArrow,      SDLK_DOWN},
	{ImGuiKey_PageUp,         SDLK_PAGEUP},
	{ImGuiKey_PageDown,       SDLK_PAGEDOWN},
	{ImGuiKey_Home,           SDLK_HOME},
	{ImGuiKey_End,            SDLK_END},
	{ImGuiKey_Insert,         SDLK_INSERT},
	{ImGuiKey_Delete,         SDLK_DELETE},
	{ImGuiKey_Backspace,      SDLK_BACKSPACE},
	{ImGuiKey_Space,          SDLK_SPACE},
	{ImGuiKey_Enter,          SDLK_RETURN},
	{ImGuiKey_Escape,         SDLK_ESCAPE},
	{ImGuiKey_Apostrophe,     SDLK_QUOTE},
	{ImGuiKey_Comma,          SDLK_COMMA},
	{ImGuiKey_Minus,          SDLK_MINUS},
	{ImGuiKey_Period,         SDLK_PERIOD},
	{ImGuiKey_Slash,          SDLK_SLASH},
	{ImGuiKey_Semicolon,      SDLK_SEMICOLON},
	{ImGuiKey_Equal,          SDLK_EQUALS},
	{ImGuiKey_LeftBracket,    SDLK_LEFTBRACKET},
	{ImGuiKey_Backslash,      SDLK_BACKSLASH},
	{ImGuiKey_RightBracket,   SDLK_RIGHTBRACKET},
	{ImGuiKey_GraveAccent,    SDLK_BACKQUOTE},
	{ImGuiKey_CapsLock,       SDLK_CAPSLOCK},
	{ImGuiKey_ScrollLock,     SDLK_SCROLLLOCK},
	{ImGuiKey_NumLock,        SDLK_NUMLOCKCLEAR},
	{ImGuiKey_PrintScreen,    SDLK_PRINTSCREEN},
	{ImGuiKey_Pause,          SDLK_PAUSE},
	{ImGuiKey_Keypad0,        SDLK_KP_0},
	{ImGuiKey_Keypad1,        SDLK_KP_1},
	{ImGuiKey_Keypad2,        SDLK_KP_2},
	{ImGuiKey_Keypad3,        SDLK_KP_3},
	{ImGuiKey_Keypad4,        SDLK_KP_4},
	{ImGuiKey_Keypad5,        SDLK_KP_5},
	{ImGuiKey_Keypad6,        SDLK_KP_6},
	{ImGuiKey_Keypad7,        SDLK_KP_7},
	{ImGuiKey_Keypad8,        SDLK_KP_8},
	{ImGuiKey_Keypad9,        SDLK_KP_9},
	{ImGuiKey_KeypadDecimal,  SDLK_KP_PERIOD},
	{ImGuiKey_KeypadDivide,   SDLK_KP_DIVIDE},
	{ImGuiKey_KeypadMultiply, SDLK_KP_MULTIPLY},
	{ImGuiKey_KeypadSubtract, SDLK_KP_MINUS},
	{ImGuiKey_KeypadAdd,      SDLK_KP_PLUS},
	{ImGuiKey_KeypadEnter,    SDLK_KP_ENTER},
	{ImGuiKey_KeypadEqual,    SDLK_KP_EQUALS},
	{ImGuiKey_LeftCtrl,       SDLK_LCTRL},
	{ImGuiKey_LeftShift,      SDLK_LSHIFT},
	{ImGuiKey_LeftAlt,        SDLK_LALT},
	{ImGuiKey_LeftSuper,      SDLK_LGUI},
	{ImGuiKey_RightCtrl,      SDLK_RCTRL},
	{ImGuiKey_RightShift,     SDLK_RSHIFT},
	{ImGuiKey_RightAlt,       SDLK_RALT},
	{ImGuiKey_RightSuper,     SDLK_RGUI},
	{ImGuiKey_Menu,           SDLK_APPLICATION},
	{ImGuiKey_0,              SDLK_0},
	{ImGuiKey_1,              SDLK_1},
	{ImGuiKey_2,              SDLK_2},
	{ImGuiKey_3,              SDLK_3},
	{ImGuiKey_4,              SDLK_4},
	{ImGuiKey_5,              SDLK_5},
	{ImGuiKey_6,              SDLK_6},
	{ImGuiKey_7,              SDLK_7},
	{ImGuiKey_8,              SDLK_8},
	{ImGuiKey_9,              SDLK_9},
	{ImGuiKey_A,              SDLK_a},
	{ImGuiKey_B,              SDLK_b},
	{ImGuiKey_C,              SDLK_c},
	{ImGuiKey_D,              SDLK_d},
	{ImGuiKey_E,              SDLK_e},
	{ImGuiKey_F,              SDLK_f},
	{ImGuiKey_G,              SDLK_g},
	{ImGuiKey_H,              SDLK_h},
	{ImGuiKey_I,              SDLK_i},
	{ImGuiKey_J,              SDLK_j},
	{ImGuiKey_K,              SDLK_k},
	{ImGuiKey_L,              SDLK_l},
	{ImGuiKey_M,              SDLK_m},
	{ImGuiKey_N,              SDLK_n},
	{ImGuiKey_O,              SDLK_o},
	{ImGuiKey_P,              SDLK_p},
	{ImGuiKey_Q,              SDLK_q},
	{ImGuiKey_R,              SDLK_r},
	{ImGuiKey_S,              SDLK_s},
	{ImGuiKey_T,              SDLK_t},
	{ImGuiKey_U,              SDLK_u},
	{ImGuiKey_V,              SDLK_v},
	{ImGuiKey_W,              SDLK_w},
	{ImGuiKey_X,              SDLK_x},
	{ImGuiKey_Y,              SDLK_y},
	{ImGuiKey_Z,              SDLK_z},
	{ImGuiKey_F1,             SDLK_F1},
	{ImGuiKey_F2,             SDLK_F2},
	{ImGuiKey_F3,             SDLK_F3},
	{ImGuiKey_F4,             SDLK_F4},
	{ImGuiKey_F5,             SDLK_F5},
	{ImGuiKey_F6,             SDLK_F6},
	{ImGuiKey_F7,             SDLK_F7},
	{ImGuiKey_F8,             SDLK_F8},
	{ImGuiKey_F9,             SDLK_F9},
	{ImGuiKey_F10,            SDLK_F10},
	{ImGuiKey_F11,            SDLK_F11},
	{ImGuiKey_F12,            SDLK_F12},
	{ImGuiKey_F13,            SDLK_F13},
	{ImGuiKey_F14,            SDLK_F14},
	{ImGuiKey_F15,            SDLK_F15},
	{ImGuiKey_F16,            SDLK_F16},
	{ImGuiKey_F17,            SDLK_F17},
	{ImGuiKey_F18,            SDLK_F18},
	{ImGuiKey_F19,            SDLK_F19},
	{ImGuiKey_F20,            SDLK_F20},
	{ImGuiKey_F21,            SDLK_F21},
	{ImGuiKey_F22,            SDLK_F22},
	{ImGuiKey_F23,            SDLK_F23},
	{ImGuiKey_F24,            SDLK_F24},
	{ImGuiKey_AppBack,        SDLK_AC_BACK},
	{ImGuiKey_AppForward,     SDLK_AC_FORWARD},
	// ... order doesn't matter
});

[[nodiscard]] inline SDL_Keycode ImGuiKey2SDL(ImGuiKey imgui)
{
	static constexpr auto lookup = []{
		auto result = unsortedKeys;
		std::ranges::sort(result, {}, &ImGui_SDL_Key::imgui);
		return result;
	}();
	auto it = std::ranges::lower_bound(lookup, imgui, {}, &ImGui_SDL_Key::imgui);
	return ((it != lookup.end()) && (it->imgui == imgui)) ? it->sdl : SDLK_UNKNOWN;
}

[[nodiscard]] inline ImGuiKey SDLKey2ImGui(SDL_Keycode sdl)
{
	static constexpr auto lookup = []{
		auto result = unsortedKeys;
		std::ranges::sort(result, {}, &ImGui_SDL_Key::sdl);
		return result;
	}();
	auto it = std::ranges::lower_bound(lookup, sdl, {}, &ImGui_SDL_Key::sdl);
	return ((it != lookup.end()) && (it->sdl == sdl)) ? it->imgui : ImGuiKey_None;
}

} // namespace openmsx

#endif
