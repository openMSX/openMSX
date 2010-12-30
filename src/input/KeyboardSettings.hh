// $Id$

#ifndef KEYBOARDSETTINGS_HH
#define KEYBOARDSETTINGS_HH

#include "Keys.hh"
#include <memory>

namespace openmsx {

class CommandController;
class BooleanSetting;
template <typename T> class EnumSetting;

class KeyboardSettings
{
public:
	enum KpEnterMode { MSX_KP_COMMA, MSX_ENTER };
	enum MappingMode { KEY_MAPPING, CHARACTER_MAPPING };

	explicit KeyboardSettings(CommandController& commandController);
	~KeyboardSettings();

	Keys::KeyCode getDeadkeyHostKey(int n);
	EnumSetting<Keys::KeyCode>& getCodeKanaHostKey();
	EnumSetting<KpEnterMode>& getKpEnterMode();
	EnumSetting<MappingMode>& getMappingMode();
	BooleanSetting& getAlwaysEnableKeypad();
	BooleanSetting& getTraceKeyPresses();
	BooleanSetting& getAutoToggleCodeKanaLock();

private:
	std::auto_ptr<EnumSetting<Keys::KeyCode> > deadkey1HostKey;
	std::auto_ptr<EnumSetting<Keys::KeyCode> > deadkey2HostKey;
	std::auto_ptr<EnumSetting<Keys::KeyCode> > deadkey3HostKey;
	std::auto_ptr<EnumSetting<Keys::KeyCode> > codeKanaHostKey;
	std::auto_ptr<EnumSetting<KpEnterMode> > kpEnterMode;
	std::auto_ptr<EnumSetting<MappingMode> > mappingMode;
	std::auto_ptr<BooleanSetting> alwaysEnableKeypad;
	std::auto_ptr<BooleanSetting> traceKeyPresses;
	std::auto_ptr<BooleanSetting> autoToggleCodeKanaLock;
};

} // namespace openmsx

#endif
