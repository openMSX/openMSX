// $Id: KeyboardSettings.cc 7234 2007-11-04 20:52:41Z awulms $

#include "KeyboardSettings.hh"
#include "Setting.hh"
#include "EnumSetting.hh"
#include "FilenameSetting.hh"
#include "BooleanSetting.hh"
#include "MSXCommandController.hh"

namespace openmsx {

KeyboardSettings::KeyboardSettings(MSXCommandController& msxCommandController)
	: keymapFile(new FilenameSetting(msxCommandController,
		"keymap-filename",
		"File with mapping from Host key codes to MSX key codes",
		""))
	, alwaysEnableKeypad(new BooleanSetting(msxCommandController,
		"always_enable_keypad",
		"Always enable keypad, even on an MSX that does not have one",
		false))
	, traceKeyPresses(new BooleanSetting(msxCommandController,
		"trace_key_presses",
		"Trace key presses (show SDL key code, SDL modifiers and Unicode code-point value)",
		false, Setting::DONT_SAVE))
	, autoToggleCodeKanaLock(new BooleanSetting(msxCommandController,
		"auto_toggle_code_kana_lock",
		"Automatically toggle the CODE/KANA lock, based on the characters entered on the host keyboard",
		false))
{
	EnumSetting<Keys::KeyCode>::Map allowedKeys;
	allowedKeys["RALT"]   = Keys::K_RALT;
	allowedKeys["RSHIFT"] = Keys::K_RSHIFT;
	allowedKeys["RMETA"]  = Keys::K_RMETA;
	allowedKeys["LMETA"]  = Keys::K_LMETA;
	allowedKeys["LSUPER"] = Keys::K_LSUPER;
	allowedKeys["RSUPER"] = Keys::K_RSUPER;
	allowedKeys["HELP"]   = Keys::K_HELP;
	allowedKeys["MENU"]   = Keys::K_MENU;
	allowedKeys["UNDO"]   = Keys::K_UNDO;
	codeKanaHostKey.reset(new EnumSetting<Keys::KeyCode>(
		msxCommandController, "code_kana_host_key",
		"Host key that maps to the MSX CODE/KANA key",
		Keys::K_RALT, allowedKeys));

	EnumSetting<KpEnterMode>::Map kpEnterModeMap;
	kpEnterModeMap["KEYPAD_COMMA"] = MSX_KP_COMMA;
	kpEnterModeMap["ENTER"] = MSX_ENTER;
	kpEnterMode.reset(new EnumSetting<KpEnterMode>(
		msxCommandController, "keypad_enter_key",
		"MSX key that the enter key on the host key pad must map to",
		MSX_KP_COMMA, kpEnterModeMap));

	EnumSetting<MappingMode>::Map mappingModeMap;
	mappingModeMap["KEY"] = KEY_MAPPING;
	mappingModeMap["CHARACTER"] = CHARACTER_MAPPING;
	mappingMode.reset(new EnumSetting<MappingMode>(
		msxCommandController, "keyboard_mapping_mode",
		"Keyboard mapping mode",
		CHARACTER_MAPPING, mappingModeMap));
}

KeyboardSettings::~KeyboardSettings()
{
}

EnumSetting<Keys::KeyCode>& KeyboardSettings::getCodeKanaHostKey()
{
	return *codeKanaHostKey;
}

EnumSetting<KeyboardSettings::KpEnterMode>& KeyboardSettings::getKpEnterMode()
{
	return *kpEnterMode;
}

EnumSetting<KeyboardSettings::MappingMode>& KeyboardSettings::getMappingMode()
{
	return *mappingMode;
}

FilenameSetting& KeyboardSettings::getKeymapFile()
{
	return *keymapFile;
}

BooleanSetting& KeyboardSettings::getAlwaysEnableKeypad()
{
	return *alwaysEnableKeypad;
}

BooleanSetting& KeyboardSettings::getTraceKeyPresses()
{
	return *traceKeyPresses;
}

BooleanSetting& KeyboardSettings::getAutoToggleCodeKanaLock()
{
	return *autoToggleCodeKanaLock;
}

} // namespace openmsx
