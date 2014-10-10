#ifndef GLOBALSETTINGS_HH
#define GLOBALSETTINGS_HH

#include "Observer.hh"
#include "noncopyable.hh"
#include "ResampledSoundDevice.hh"
#include <memory>
#include <vector>

namespace openmsx {

class GlobalCommandController;
class IntegerSetting;
class BooleanSetting;
class StringSetting;
class ThrottleManager;
class Setting;
template <class T> class EnumSetting;

/**
 * This class contains settings that are used by several other class
 * (including some singletons). This class was introduced to solve
 * lifetime management issues.
 */
class GlobalSettings final : private Observer<Setting>, private noncopyable
{
public:
	explicit GlobalSettings(GlobalCommandController& commandController);
	~GlobalSettings();

	IntegerSetting& getSpeedSetting() const {
		return *speedSetting;
	}
	BooleanSetting& getPauseSetting() const {
		return *pauseSetting;
	}
	BooleanSetting& getPowerSetting() const {
		return *powerSetting;
	}
	BooleanSetting& getAutoSaveSetting() const {
		return *autoSaveSetting;
	}
	BooleanSetting& getPauseOnLostFocusSetting() const {
		return *pauseOnLostFocusSetting;
	}
	StringSetting& getUMRCallBackSetting() const {
		return *umrCallBackSetting;
	}
	StringSetting& getInvalidPsgDirectionsSetting() const {
		return *invalidPsgDirectionsSetting;
	}
	EnumSetting<ResampledSoundDevice::ResampleType>& getResampleSetting() const {
		return *resampleSetting;
	}
	IntegerSetting& getJoyDeadzoneSetting(int i) const {
		return *deadzoneSettings[i];
	}
	ThrottleManager& getThrottleManager() const {
		return *throttleManager;
	}

private:
	// Observer<Setting>
	void update(const Setting& setting) override;

	GlobalCommandController& commandController;

	std::unique_ptr<IntegerSetting> speedSetting;
	std::unique_ptr<BooleanSetting> pauseSetting;
	std::unique_ptr<BooleanSetting> powerSetting;
	std::unique_ptr<BooleanSetting> autoSaveSetting;
	std::unique_ptr<BooleanSetting> pauseOnLostFocusSetting;
	std::unique_ptr<StringSetting>  umrCallBackSetting;
	std::unique_ptr<StringSetting>  invalidPsgDirectionsSetting;
	std::unique_ptr<EnumSetting<ResampledSoundDevice::ResampleType>> resampleSetting;
	std::vector<std::unique_ptr<IntegerSetting>> deadzoneSettings;
	std::unique_ptr<ThrottleManager> throttleManager;
};

} // namespace openmsx

#endif
