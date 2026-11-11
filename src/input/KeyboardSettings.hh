#ifndef KEYBOARDSETTINGS_HH
#define KEYBOARDSETTINGS_HH

#include "BooleanSetting.hh"
#include "EnumSetting.hh"

#include <cstdint>

namespace openmsx {

class CommandController;

class KeyboardSettings
{
public:
	enum class MappingMode : uint8_t { KEY, CHARACTER, POSITIONAL };

	explicit KeyboardSettings(CommandController& commandController);

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
#if defined(_WIN32)
	[[nodiscard]] bool getDisableIME() const {
		return disableIME.getBoolean();
	}
#endif

private:
	EnumSetting<MappingMode> mappingMode;
	BooleanSetting alwaysEnableKeypad;
	BooleanSetting traceKeyPresses;
	BooleanSetting autoToggleCodeKanaLock;
#if defined(_WIN32)
	BooleanSetting disableIME;
#endif
};

} // namespace openmsx

#endif
