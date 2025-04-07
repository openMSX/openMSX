#ifndef GLOBALSETTINGS_HH
#define GLOBALSETTINGS_HH

#include "Observer.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "StringSetting.hh"
#include "SpeedManager.hh"
#include "TclCallback.hh"
#include "ThrottleManager.hh"
#include "ResampledSoundDevice.hh"
#include <memory>
#include <vector>

namespace openmsx {

class GlobalCommandController;

/**
 * This class contains settings that are used by several other class
 * (including some singletons). This class was introduced to solve
 * lifetime management issues.
 */
class GlobalSettings final : private Observer<Setting>
{
public:
	explicit GlobalSettings(GlobalCommandController& commandController);
	~GlobalSettings();

	[[nodiscard]] BooleanSetting& getPauseSetting() {
		return pauseSetting;
	}
	[[nodiscard]] BooleanSetting& getPowerSetting() {
		return powerSetting;
	}
	[[nodiscard]] BooleanSetting& getAutoSaveSetting() {
		return autoSaveSetting;
	}
	[[nodiscard]] TclCallback& getExitCallBackSetting() {
		return exitCallBackSetting;
	}
	[[nodiscard]] StringSetting& getUMRCallBackSetting() {
		return umrCallBackSetting;
	}
	[[nodiscard]] StringSetting& getInvalidPsgDirectionsSetting() {
		return invalidPsgDirectionsSetting;
	}
	[[nodiscard]] StringSetting& getInvalidPpiModeSetting() {
		return invalidPpiModeSetting;
	}
	[[nodiscard]] EnumSetting<ResampledSoundDevice::ResampleType>& getResampleSetting() {
		return resampleSetting;
	}
	[[nodiscard]] SpeedManager& getSpeedManager() {
		return speedManager;
	}
	[[nodiscard]] ThrottleManager& getThrottleManager() {
		return throttleManager;
	}

private:
	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	GlobalCommandController& commandController;

	BooleanSetting pauseSetting;
	BooleanSetting powerSetting;
	BooleanSetting autoSaveSetting;
	TclCallback    exitCallBackSetting;
	StringSetting  umrCallBackSetting;
	StringSetting  invalidPsgDirectionsSetting;
	StringSetting  invalidPpiModeSetting;
	EnumSetting<ResampledSoundDevice::ResampleType> resampleSetting;
	SpeedManager speedManager;
	ThrottleManager throttleManager;
};

} // namespace openmsx

#endif
