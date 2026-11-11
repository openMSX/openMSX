#include "KeyboardSettings.hh"

namespace openmsx {

KeyboardSettings::KeyboardSettings(CommandController& commandController)
	: mappingMode(commandController,
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
		false, Setting::Save::NO)
	, autoToggleCodeKanaLock(commandController,
		"kbd_auto_toggle_code_kana_lock",
		"Automatically toggle the CODE/KANA lock, based on the characters entered on the host keyboard",
		true)
#if defined(_WIN32)
	, disableIME(commandController,
		"kbd_disable_IME",
		"disable IME, avoid keyboard input problems in Kanji mode",
		true)
#endif
{
}

} // namespace openmsx
