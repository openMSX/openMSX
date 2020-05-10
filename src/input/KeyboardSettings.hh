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
	enum MappingMode { KEY_MAPPING, CHARACTER_MAPPING, POSITIONAL_MAPPING };

	explicit KeyboardSettings(CommandController& commandController);

	Keys::KeyCode getDeadkeyHostKey(unsigned n) const {
		assert(n < 3);
		return deadkeyHostKey[n]->getEnum();
	}
	Keys::KeyCode getCodeKanaHostKey() const {
		return codeKanaHostKey.getEnum();
	}
	KpEnterMode getKpEnterMode() const {
		return kpEnterMode.getEnum();
	}
	MappingMode getMappingMode() const {
		return mappingMode.getEnum();
	}
	bool getAlwaysEnableKeypad() const {
		return alwaysEnableKeypad.getBoolean();
	}
	bool getTraceKeyPresses() const {
		return traceKeyPresses.getBoolean();
	}
	bool getAutoToggleCodeKanaLock() const {
		return autoToggleCodeKanaLock.getBoolean();
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
