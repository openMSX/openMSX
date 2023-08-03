#ifndef GLOBALSETTINGS_HH
#define GLOBALSETTINGS_HH

#include "Observer.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "StringSetting.hh"
#include "SpeedManager.hh"
#include "ThrottleManager.hh"
#include "ResampledSoundDevice.hh"

#ifdef _WIN32
#include "SocketSettingsManager.hh"
#endif

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
	[[nodiscard]] IntegerSetting& getJoyDeadZoneSetting(int i) {
		return *deadZoneSettings[i];
	}
	[[nodiscard]] SpeedManager& getSpeedManager() {
		return speedManager;
	}
	[[nodiscard]] ThrottleManager& getThrottleManager() {
		return throttleManager;
	}
#ifdef _WIN32
	[[nodiscard]] SocketSettingsManager& getSocketSettingsManager() {
		return socketSettingsManager;
	}
#endif

private:
	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	GlobalCommandController& commandController;

	BooleanSetting pauseSetting;
	BooleanSetting powerSetting;
	BooleanSetting autoSaveSetting;
	StringSetting  umrCallBackSetting;
	StringSetting  invalidPsgDirectionsSetting;
	StringSetting  invalidPpiModeSetting;
	EnumSetting<ResampledSoundDevice::ResampleType> resampleSetting;
	std::vector<std::unique_ptr<IntegerSetting>> deadZoneSettings;
	SpeedManager speedManager;
	ThrottleManager throttleManager;
#ifdef _WIN32
	SocketSettingsManager socketSettingsManager;
#endif
};

} // namespace openmsx

#endif
