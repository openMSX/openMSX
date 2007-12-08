// $Id: KeyboardSettings.cc 7234 2007-11-04 20:52:41Z awulms $

#include "KeyboardSettings.hh"
#include "Setting.hh"
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
}

KeyboardSettings::~KeyboardSettings()
{
}

EnumSetting<Keys::KeyCode>& KeyboardSettings::getCodeKanaHostKey()
{
	return *codeKanaHostKey.get();
}

EnumSetting<KeyboardSettings::KpEnterMode>& KeyboardSettings::getKpEnterMode()
{
	return *kpEnterMode.get();
}

FilenameSetting& KeyboardSettings::getKeymapFile()
{
	return *keymapFile.get();
}

BooleanSetting& KeyboardSettings::getAlwaysEnableKeypad()
{
	return *alwaysEnableKeypad.get();
}

BooleanSetting& KeyboardSettings::getTraceKeyPresses()
{
	return *traceKeyPresses.get();
}

} // namespace openmsx
