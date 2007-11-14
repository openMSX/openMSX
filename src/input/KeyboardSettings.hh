// $Id: KeyboardSettings.hh 7235 2007-11-04 21:24:10Z awulms $

#ifndef KEYBOARDSETTINGS_HH
#define KEYBOARDSETTINGS_HH

#include "Keys.hh"
#include <memory>

#include "EnumSetting.hh"
#include "FilenameSetting.hh"
#include "BooleanSetting.hh"

namespace openmsx {

class MSXCommandController;

class KeyboardSettings
{
public:
	enum KpEnterMode { MSX_KP_COMMA, MSX_ENTER };
	KeyboardSettings(MSXCommandController& msxCommandController);
	virtual ~KeyboardSettings();

	EnumSetting<Keys::KeyCode>& getCodeKanaHostKey();
	EnumSetting<KpEnterMode>& getKpEnterMode();
	FilenameSetting& getKeymapFile();
	BooleanSetting& getAlwaysEnableKeypad();
	BooleanSetting& getTraceKeyPresses();
private:
	std::auto_ptr<EnumSetting<Keys::KeyCode> > codeKanaHostKey;
	std::auto_ptr<EnumSetting<KpEnterMode> > kpEnterMode;
	std::auto_ptr<FilenameSetting> keymapFile;
	std::auto_ptr<BooleanSetting> alwaysEnableKeypad;
	std::auto_ptr<BooleanSetting> traceKeyPresses;
};

} // namespace openmsx

#endif
