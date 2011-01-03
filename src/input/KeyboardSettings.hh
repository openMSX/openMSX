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

	Keys::KeyCode getDeadkeyHostKey(unsigned n) const;
	EnumSetting<Keys::KeyCode>& getCodeKanaHostKey() const;
	EnumSetting<KpEnterMode>& getKpEnterMode() const;
	EnumSetting<MappingMode>& getMappingMode() const;
	BooleanSetting& getAlwaysEnableKeypad() const;
	BooleanSetting& getTraceKeyPresses() const;
	BooleanSetting& getAutoToggleCodeKanaLock() const;

private:
	std::auto_ptr<EnumSetting<Keys::KeyCode> > deadkeyHostKey[3];
	std::auto_ptr<EnumSetting<Keys::KeyCode> > codeKanaHostKey;
	std::auto_ptr<EnumSetting<KpEnterMode> > kpEnterMode;
	std::auto_ptr<EnumSetting<MappingMode> > mappingMode;
	std::auto_ptr<BooleanSetting> alwaysEnableKeypad;
	std::auto_ptr<BooleanSetting> traceKeyPresses;
	std::auto_ptr<BooleanSetting> autoToggleCodeKanaLock;
};

} // namespace openmsx

#endif
