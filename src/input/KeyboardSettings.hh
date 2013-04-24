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
	std::unique_ptr<EnumSetting<Keys::KeyCode>> deadkeyHostKey[3];
	std::unique_ptr<EnumSetting<Keys::KeyCode>> codeKanaHostKey;
	std::unique_ptr<EnumSetting<KpEnterMode>> kpEnterMode;
	std::unique_ptr<EnumSetting<MappingMode>> mappingMode;
	std::unique_ptr<BooleanSetting> alwaysEnableKeypad;
	std::unique_ptr<BooleanSetting> traceKeyPresses;
	std::unique_ptr<BooleanSetting> autoToggleCodeKanaLock;
};

} // namespace openmsx

#endif
