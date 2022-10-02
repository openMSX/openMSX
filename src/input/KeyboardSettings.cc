#include "KeyboardSettings.hh"
#include "stl.hh"
#include "strCat.hh"
#include <array>

namespace openmsx {

[[nodiscard]] static EnumSetting<Keys::KeyCode>::Map getAllowedKeysMap()
{
	return {
		{"RALT",        Keys::K_RALT},
		{"MENU",        Keys::K_MENU},
		{"RCTRL",       Keys::K_RCTRL},
		{"HENKAN_MODE", Keys::K_HENKAN_MODE},
		{"RSHIFT",      Keys::K_RSHIFT},
		{"RMETA",       Keys::K_RSUPER}, // TODO correct???
		{"LMETA",       Keys::K_LSUPER}, //
		{"LSUPER",      Keys::K_LSUPER},
		{"RSUPER",      Keys::K_RSUPER},
		{"HELP",        Keys::K_HELP},
		{"UNDO",        Keys::K_UNDO},
		{"END",         Keys::K_END},
		{"PAGEUP",      Keys::K_PAGEUP},
		{"PAGEDOWN",    Keys::K_PAGEDOWN}
	};
}

KeyboardSettings::KeyboardSettings(CommandController& commandController)
	: deadkeyHostKey(generate_array<3>([&](auto i) {
		static constexpr std::array<static_string_view, 3> description = {
			"Host key that maps to deadkey 1. Not applicable to Japanese and Korean MSX models",
			"Host key that maps to deadkey 2. Only applicable to Brazilian MSX models (Sharp Hotbit and Gradiente)",
			"Host key that maps to deadkey 3. Only applicable to Brazilian Sharp Hotbit MSX models",
		};
		static constexpr std::array<Keys::KeyCode, 3> defaultKey = {
			Keys::K_RCTRL, Keys::K_PAGEUP, Keys::K_PAGEDOWN,
		};
		return EnumSetting<Keys::KeyCode>(
			commandController, tmpStrCat("kbd_deadkey", i + 1, "_host_key"),
			description[i], defaultKey[i], getAllowedKeysMap());
	}))
	, codeKanaHostKey(commandController,
		"kbd_code_kana_host_key",
		"Host key that maps to the MSX CODE/KANA key. Please note that the HENKAN_MODE key only exists on Japanese host keyboards)",
		Keys::K_RALT, getAllowedKeysMap())
	, kpEnterMode(commandController,
		"kbd_numkeypad_enter_key",
		"MSX key that the enter key on the host numeric keypad must map to",
		MSX_KP_COMMA, EnumSetting<KpEnterMode>::Map{
			{"KEYPAD_COMMA", MSX_KP_COMMA},
			{"ENTER",        MSX_ENTER}})
	, mappingMode(commandController,
		"kbd_mapping_mode",
		"Keyboard mapping mode",
		CHARACTER_MAPPING, EnumSetting<MappingMode>::Map{
			{"KEY",        KEY_MAPPING},
			{"CHARACTER",  CHARACTER_MAPPING},
			{"POSITIONAL", POSITIONAL_MAPPING}})
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
