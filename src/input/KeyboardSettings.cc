#include "KeyboardSettings.hh"
#include "stl.hh"
#include "strCat.hh"
#include <array>

namespace openmsx {

[[nodiscard]] static EnumSetting<SDL_Keycode>::Map getAllowedKeysMap()
{
	return {
		{"RALT",        SDLK_RALT},
		{"MENU",        SDLK_MENU},
		{"RCTRL",       SDLK_RCTRL},
		//{"HENKAN_MODE", Keys::K_HENKAN_MODE},
		{"RSHIFT",      SDLK_RSHIFT},
		{"RGUI",        SDLK_RGUI},
		{"LGUI",        SDLK_LGUI},
		{"RMETA",       SDLK_RGUI}, // bw-compat
		{"LMETA",       SDLK_LGUI}, // "
		{"LSUPER",      SDLK_LGUI}, // "
		{"RSUPER",      SDLK_RGUI}, // "
		{"HELP",        SDLK_HELP},
		{"UNDO",        SDLK_UNDO},
		{"END",         SDLK_END},
		{"PAGEUP",      SDLK_PAGEUP},
		{"PAGEDOWN",    SDLK_PAGEDOWN}
	};
}

KeyboardSettings::KeyboardSettings(CommandController& commandController)
	: deadKeyHostKey(generate_array<3>([&](auto i) {
		static constexpr std::array<static_string_view, 3> description = {
			"Host key that maps to deadkey 1. Not applicable to Japanese and Korean MSX models",
			"Host key that maps to deadkey 2. Only applicable to Brazilian MSX models (Sharp Hotbit and Gradiente)",
			"Host key that maps to deadkey 3. Only applicable to Brazilian Sharp Hotbit MSX models",
		};
		static constexpr std::array<SDL_Keycode, 3> defaultKey = {
			SDLK_RCTRL, SDLK_PAGEUP, SDLK_PAGEDOWN,
		};
		return EnumSetting<SDL_Keycode>(
			commandController, tmpStrCat("kbd_deadkey", i + 1, "_host_key"),
			description[i], defaultKey[i], getAllowedKeysMap());
	}))
	, codeKanaHostKey(commandController,
		"kbd_code_kana_host_key",
		"Host key that maps to the MSX CODE/KANA key. Please note that the HENKAN_MODE key only exists on Japanese host keyboards)",
		SDLK_RALT, getAllowedKeysMap())
	, kpEnterMode(commandController,
		"kbd_numkeypad_enter_key",
		"MSX key that the enter key on the host numeric keypad must map to",
		KpEnterMode::MSX_KP_COMMA, EnumSetting<KpEnterMode>::Map{
			{"KEYPAD_COMMA", KpEnterMode::MSX_KP_COMMA},
			{"ENTER",        KpEnterMode::MSX_ENTER}})
	, mappingMode(commandController,
		"kbd_mapping_mode",
		"Keyboard mapping mode",
		MappingMode::CHARACTER, EnumSetting<MappingMode>::Map{
			{"KEY",        MappingMode::KEY},
			{"CHARACTER",  MappingMode::CHARACTER},
			{"POSITIONAL", MappingMode::POSITIONAL}})
	, alwaysEnableKeypad(commandController,
		"kbd_numkeypad_always_enabled",
		"Numeric keypad is always enabled, even on an MSX that does not have one",
		false)
	, traceKeyPresses(commandController,
		"kbd_trace_key_presses",
		"Trace key presses (show SDL key code, SDL modifiers and Unicode code-point value)",
		false, Setting::DONT_SAVE)
	, autoToggleCodeKanaLock(commandController,
		"kbd_auto_toggle_code_kana_lock",
		"Automatically toggle the CODE/KANA lock, based on the characters entered on the host keyboard",
		true)
{
}

} // namespace openmsx
