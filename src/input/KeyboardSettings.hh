#ifndef KEYBOARDSETTINGS_HH
#define KEYBOARDSETTINGS_HH

#include "Keys.hh"
#include "EnumSetting.hh"
#include "BooleanSetting.hh"
#include <memory>
#include <cassert>

namespace openmsx {

class CommandController;

class KeyboardSettings
{
public:
	enum KpEnterMode { MSX_KP_COMMA, MSX_ENTER };
	enum MappingMode { KEY_MAPPING, CHARACTER_MAPPING };

	explicit KeyboardSettings(CommandController& commandController);

	Keys::KeyCode getDeadkeyHostKey(unsigned n) const {
		assert(n < 3);
		return deadkeyHostKey[n]->getEnum();
	}

	EnumSetting<Keys::KeyCode>& getCodeKanaHostKey() {
		return codeKanaHostKey;
	}
	EnumSetting<KpEnterMode>& getKpEnterMode() {
		return kpEnterMode;
	}
	EnumSetting<MappingMode>& getMappingMode() {
		return mappingMode;
	}
	BooleanSetting& getAlwaysEnableKeypad() {
		return alwaysEnableKeypad;
	}
	BooleanSetting& getTraceKeyPresses() {
		return traceKeyPresses;
	}
	BooleanSetting& getAutoToggleCodeKanaLock() {
		return autoToggleCodeKanaLock;
	}

private:
	std::unique_ptr<EnumSetting<Keys::KeyCode>> deadkeyHostKey[3];
	EnumSetting<Keys::KeyCode> codeKanaHostKey;
	EnumSetting<KpEnterMode> kpEnterMode;
	EnumSetting<MappingMode> mappingMode;
	BooleanSetting alwaysEnableKeypad;
	BooleanSetting traceKeyPresses;
	BooleanSetting autoToggleCodeKanaLock;
};

} // namespace openmsx

#endif
