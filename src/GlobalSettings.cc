// $Id$

#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "StringSetting.hh"
#include "SettingsConfig.hh"
#include "CommandController.hh"
#include "GlobalCommandController.hh"
#include "ThrottleManager.hh"
#include "EnumSetting.hh"
#include "build-info.hh"

namespace openmsx {

GlobalSettings::GlobalSettings(GlobalCommandController& commandController_)
	: commandController(commandController_)
{
	speedSetting.reset(new IntegerSetting(commandController, "speed",
	       "controls the emulation speed: higher is faster, 100 is normal",
	       100, 1, 1000000));
	pauseSetting.reset(new BooleanSetting(commandController, "pause",
	       "pauses the emulation", false, Setting::DONT_SAVE));
	powerSetting.reset(new BooleanSetting(commandController, "power",
	        "turn power on/off", false, Setting::DONT_SAVE));
	autoSaveSetting.reset(new BooleanSetting(commandController,
	        "save_settings_on_exit",
	        "automatically save settings when openMSX exits", true));
	pauseOnLostFocusSetting.reset(new BooleanSetting(commandController, "pause_on_lost_focus",
	       "pause emulation when the openMSX window loses focus", false));
	umrCallBackSetting.reset(new StringSetting(commandController,
	        "umr_callback", "Tcl proc to call when an UMR is detected", ""));
	EnumSetting<ResampledSoundDevice::ResampleType>::Map resampleMap;
	resampleMap["hq"]   = ResampledSoundDevice::RESAMPLE_HQ;
	resampleMap["fast"] = ResampledSoundDevice::RESAMPLE_LQ;
	resampleMap["blip"] = ResampledSoundDevice::RESAMPLE_BLIP;
	resampleSetting.reset(new EnumSetting<ResampledSoundDevice::ResampleType>(
		commandController, "resampler", "Resample algorithm",
#if PLATFORM_DINGUX
		ResampledSoundDevice::RESAMPLE_LQ,
#else
		ResampledSoundDevice::RESAMPLE_BLIP,
#endif
		resampleMap));

	throttleManager.reset(new ThrottleManager(commandController));

	getPowerSetting().attach(*this);
}

GlobalSettings::~GlobalSettings()
{
	getPowerSetting().detach(*this);
	commandController.getSettingsConfig().setSaveSettings(
		autoSaveSetting->getValue());
}

IntegerSetting& GlobalSettings::getSpeedSetting()
{
	return *speedSetting.get();
}

BooleanSetting& GlobalSettings::getPauseSetting()
{
	return *pauseSetting.get();
}

BooleanSetting& GlobalSettings::getPowerSetting()
{
	return *powerSetting.get();
}

BooleanSetting& GlobalSettings::getAutoSaveSetting()
{
	return *autoSaveSetting.get();
}

ThrottleManager& GlobalSettings::getThrottleManager()
{
	return *throttleManager.get();
}

StringSetting& GlobalSettings::getUMRCallBackSetting()
{
	return *umrCallBackSetting.get();
}

BooleanSetting& GlobalSettings::getPauseOnLostFocusSetting()
{
	return *pauseOnLostFocusSetting.get();
}

EnumSetting<ResampledSoundDevice::ResampleType>& GlobalSettings::getResampleSetting()
{
	return *resampleSetting.get();
}

// Observer<Setting>
void GlobalSettings::update(const Setting& setting)
{
	if (&setting == &getPowerSetting()) { // either on or off
		// automatically unpause after a power off/on cycle
		// this solved a bug, but apart from that this behaviour also
		// makes more sense
		getPauseSetting().changeValue(false);
	}
}

} // namespace openmsx
