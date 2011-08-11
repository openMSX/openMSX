// $Id$

#ifndef GLOBALSETTINGS_HH
#define GLOBALSETTINGS_HH

#include "Observer.hh"
#include "noncopyable.hh"
#include "ResampledSoundDevice.hh"
#include <memory>

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
class GlobalSettings : private Observer<Setting>, private noncopyable
{
public:
	explicit GlobalSettings(GlobalCommandController& commandController);
	~GlobalSettings();

	IntegerSetting& getSpeedSetting();
	BooleanSetting& getPauseSetting();
	BooleanSetting& getPowerSetting();
	BooleanSetting& getAutoSaveSetting();
	BooleanSetting& getPauseOnLostFocusSetting();
	StringSetting&  getUMRCallBackSetting();
	EnumSetting<ResampledSoundDevice::ResampleType>& getResampleSetting();
	ThrottleManager& getThrottleManager();

private:
	// Observer<Setting>
	virtual void update(const Setting& setting);

	GlobalCommandController& commandController;

	std::auto_ptr<IntegerSetting> speedSetting;
	std::auto_ptr<BooleanSetting> pauseSetting;
	std::auto_ptr<BooleanSetting> powerSetting;
	std::auto_ptr<BooleanSetting> autoSaveSetting;
	std::auto_ptr<BooleanSetting> pauseOnLostFocusSetting;
	std::auto_ptr<StringSetting>  umrCallBackSetting;
	std::auto_ptr<EnumSetting<ResampledSoundDevice::ResampleType> > resampleSetting;
	std::auto_ptr<ThrottleManager> throttleManager;
};

} // namespace openmsx

#endif
