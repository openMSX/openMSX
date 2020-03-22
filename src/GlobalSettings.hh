#ifndef GLOBALSETTINGS_HH
#define GLOBALSETTINGS_HH

#include "Observer.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "StringSetting.hh"
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

	IntegerSetting& getSpeedSetting() {
		return speedSetting;
	}
	BooleanSetting& getPauseSetting() {
		return pauseSetting;
	}
	BooleanSetting& getPowerSetting() {
		return powerSetting;
	}
	BooleanSetting& getAutoSaveSetting() {
		return autoSaveSetting;
	}
	StringSetting& getUMRCallBackSetting() {
		return umrCallBackSetting;
	}
	StringSetting& getInvalidPsgDirectionsSetting() {
		return invalidPsgDirectionsSetting;
	}
	StringSetting& getInvalidPpiModeSetting() {
		return invalidPpiModeSetting;
	}
	EnumSetting<ResampledSoundDevice::ResampleType>& getResampleSetting() {
		return resampleSetting;
	}
	IntegerSetting& getJoyDeadzoneSetting(int i) {
		return *deadzoneSettings[i];
	}
	ThrottleManager& getThrottleManager() {
		return throttleManager;
	}

private:
	// Observer<Setting>
	void update(const Setting& setting) override;

	GlobalCommandController& commandController;

	IntegerSetting speedSetting;
	BooleanSetting pauseSetting;
	BooleanSetting powerSetting;
	BooleanSetting autoSaveSetting;
	StringSetting  umrCallBackSetting;
	StringSetting  invalidPsgDirectionsSetting;
	StringSetting  invalidPpiModeSetting;
	EnumSetting<ResampledSoundDevice::ResampleType> resampleSetting;
	std::vector<std::unique_ptr<IntegerSetting>> deadzoneSettings;
	ThrottleManager throttleManager;
};

} // namespace openmsx

#endif
