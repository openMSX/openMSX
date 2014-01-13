#include "KeyboardSettings.hh"
#include "EnumSetting.hh"
#include "BooleanSetting.hh"
#include "memory.hh"
#include <cassert>

namespace openmsx {

KeyboardSettings::KeyboardSettings(CommandController& commandController)
	: alwaysEnableKeypad(make_unique<BooleanSetting>(
		commandController, "kbd_numkeypad_always_enabled",
		"Numeric keypad is always enabled, even on an MSX that does not have one",
		false))
	, traceKeyPresses(make_unique<BooleanSetting>(
		commandController, "kbd_trace_key_presses",
		"Trace key presses (show SDL key code, SDL modifiers and Unicode code-point value)",
		false, Setting::DONT_SAVE))
	, autoToggleCodeKanaLock(make_unique<BooleanSetting>(commandController,
		"kbd_auto_toggle_code_kana_lock",
		"Automatically toggle the CODE/KANA lock, based on the characters entered on the host keyboard",
		true))
{
	EnumSetting<Keys::KeyCode>::Map allowedKeys = {
		{ "RALT",        Keys::K_RALT },
		{ "MENU",        Keys::K_MENU },
		{ "RCTRL",       Keys::K_RCTRL },
		{ "HENKAN_MODE", Keys::K_HENKAN_MODE },
		{ "RSHIFT",      Keys::K_RSHIFT },
		{ "RMETA",       Keys::K_RMETA },
		{ "LMETA",       Keys::K_LMETA },
		{ "LSUPER",      Keys::K_LSUPER },
		{ "RSUPER",      Keys::K_RSUPER },
		{ "HELP",        Keys::K_HELP },
		{ "UNDO",        Keys::K_UNDO },
		{ "END",         Keys::K_END },
		{ "PAGEUP",      Keys::K_PAGEUP },
		{ "PAGEDOWN",    Keys::K_PAGEDOWN } };
	codeKanaHostKey = make_unique<EnumSetting<Keys::KeyCode>>(
		commandController, "kbd_code_kana_host_key",
		"Host key that maps to the MSX CODE/KANA key. Please note that the HENKAN_MODE key only exists on Japanese host keyboards)",
		Keys::K_RALT, allowedKeys);

	deadkeyHostKey[0] = make_unique<EnumSetting<Keys::KeyCode>>(
		commandController, "kbd_deadkey1_host_key",
		"Host key that maps to deadkey 1. Not applicable to Japanese and Korean MSX models",
		Keys::K_RCTRL, allowedKeys);

	deadkeyHostKey[1] = make_unique<EnumSetting<Keys::KeyCode>>(
		commandController, "kbd_deadkey2_host_key",
		"Host key that maps to deadkey 2. Only applicable to Brazilian MSX models (Sharp Hotbit and Gradiente)",
		Keys::K_PAGEUP, allowedKeys);

	deadkeyHostKey[2] = make_unique<EnumSetting<Keys::KeyCode>>(
		commandController, "kbd_deadkey3_host_key",
		"Host key that maps to deadkey 3. Only applicable to Brazilian Sharp Hotbit MSX models",
		Keys::K_PAGEDOWN, allowedKeys);

	EnumSetting<KpEnterMode>::Map kpEnterModeMap = {
		{ "KEYPAD_COMMA", MSX_KP_COMMA },
		{ "ENTER",        MSX_ENTER } };
	kpEnterMode = make_unique<EnumSetting<KpEnterMode>>(
		commandController, "kbd_numkeypad_enter_key",
		"MSX key that the enter key on the host numeric keypad must map to",
		MSX_KP_COMMA, kpEnterModeMap);

	EnumSetting<MappingMode>::Map mappingModeMap = {
		{ "KEY",       KEY_MAPPING },
		{ "CHARACTER", CHARACTER_MAPPING } };
	mappingMode = make_unique<EnumSetting<MappingMode>>(
		commandController, "kbd_mapping_mode",
		"Keyboard mapping mode",
		CHARACTER_MAPPING, mappingModeMap);
}

KeyboardSettings::~KeyboardSettings()
{
}

EnumSetting<Keys::KeyCode>& KeyboardSettings::getCodeKanaHostKey() const
{
	return *codeKanaHostKey;
}

Keys::KeyCode KeyboardSettings::getDeadkeyHostKey(unsigned n) const
{
	assert(n < 3);
	return deadkeyHostKey[n]->getEnum();
}

EnumSetting<KeyboardSettings::KpEnterMode>& KeyboardSettings::getKpEnterMode() const
{
	return *kpEnterMode;
}

EnumSetting<KeyboardSettings::MappingMode>& KeyboardSettings::getMappingMode() const
{
	return *mappingMode;
}

BooleanSetting& KeyboardSettings::getAlwaysEnableKeypad() const
{
	return *alwaysEnableKeypad;
}

BooleanSetting& KeyboardSettings::getTraceKeyPresses() const
{
	return *traceKeyPresses;
}

BooleanSetting& KeyboardSettings::getAutoToggleCodeKanaLock() const
{
	return *autoToggleCodeKanaLock;
}

} // namespace openmsx
