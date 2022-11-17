#ifndef KEYBOARDSETTINGS_HH
#define KEYBOARDSETTINGS_HH

#include "Keys.hh"
#include "EnumSetting.hh"
#include "BooleanSetting.hh"
#include <array>
#include <cassert>

namespace openmsx {

class CommandController;

class KeyboardSettings
{
public:
	enum KpEnterMode { MSX_KP_COMMA, MSX_ENTER };
	enum MappingMode { KEY_MAPPING, CHARACTER_MAPPING, POSITIONAL_MAPPING };

	explicit KeyboardSettings(CommandController& commandController);

	[[nodiscard]] Keys::KeyCode getDeadKeyHostKey(unsigned n) const {
		assert(n < 3);
		return deadKeyHostKey[n].getEnum();
	}
	[[nodiscard]] Keys::KeyCode getCodeKanaHostKey() const {
		return codeKanaHostKey.getEnum();
	}
	[[nodiscard]] KpEnterMode getKpEnterMode() const {
		return kpEnterMode.getEnum();
	}
	[[nodiscard]] MappingMode getMappingMode() const {
		return mappingMode.getEnum();
	}
	[[nodiscard]] bool getAlwaysEnableKeypad() const {
		return alwaysEnableKeypad.getBoolean();
	}
	[[nodiscard]] bool getTraceKeyPresses() const {
		return traceKeyPresses.getBoolean();
	}
	[[nodiscard]] bool getAutoToggleCodeKanaLock() const {
		return autoToggleCodeKanaLock.getBoolean();
	}

private:
	std::array<EnumSetting<Keys::KeyCode>, 3> deadKeyHostKey;
	EnumSetting<Keys::KeyCode> codeKanaHostKey;
	EnumSetting<KpEnterMode> kpEnterMode;
	EnumSetting<MappingMode> mappingMode;
	BooleanSetting alwaysEnableKeypad;
	BooleanSetting traceKeyPresses;
	BooleanSetting autoToggleCodeKanaLock;
};

} // namespace openmsx

#endif
