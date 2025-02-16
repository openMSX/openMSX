#ifndef KEYBOARDSETTINGS_HH
#define KEYBOARDSETTINGS_HH

#include "SDLKey.hh"
#include "EnumSetting.hh"
#include "BooleanSetting.hh"

#include <array>
#include <cassert>

namespace openmsx {

class CommandController;

class KeyboardSettings
{
public:
	enum class KpEnterMode { MSX_KP_COMMA, MSX_ENTER };
	enum class MappingMode { KEY, CHARACTER, POSITIONAL };

	explicit KeyboardSettings(CommandController& commandController);

	[[nodiscard]] SDL_Keycode getDeadKeyHostKey(unsigned n) const {
		assert(n < 3);
		return deadKeyHostKey[n].getEnum();
	}
	[[nodiscard]] SDL_Keycode getCodeKanaHostKey() const {
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
	[[nodiscard]] bool getDisableIME() const {
		return disableIME.getBoolean();
	}

private:
	std::array<EnumSetting<SDL_Keycode>, 3> deadKeyHostKey;
	EnumSetting<SDL_Keycode> codeKanaHostKey;
	EnumSetting<KpEnterMode> kpEnterMode;
	EnumSetting<MappingMode> mappingMode;
	BooleanSetting alwaysEnableKeypad;
	BooleanSetting traceKeyPresses;
	BooleanSetting autoToggleCodeKanaLock;
	BooleanSetting disableIME;
};

} // namespace openmsx

#endif
