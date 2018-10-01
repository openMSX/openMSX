#include "GlobalSettings.hh"
#include "SettingsConfig.hh"
#include "GlobalCommandController.hh"
#include "strCat.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <memory>
#include <SDL.h>

namespace openmsx {

GlobalSettings::GlobalSettings(GlobalCommandController& commandController_)
	: commandController(commandController_)
	, speedSetting(commandController, "speed",
	       "controls the emulation speed: higher is faster, 100 is normal",
	       100, 1, 1000000)
	, pauseSetting(commandController, "pause",
	       "pauses the emulation", false, Setting::DONT_SAVE)
	, powerSetting(commandController, "power",
	        "turn power on/off", false, Setting::DONT_SAVE)
	, autoSaveSetting(commandController, "save_settings_on_exit",
	        "automatically save settings when openMSX exits", true)
	, pauseOnLostFocusSetting(commandController, "pause_on_lost_focus",
	       "pause emulation when the openMSX window loses focus", false)
	, umrCallBackSetting(commandController, "umr_callback",
		"Tcl proc to call when an UMR is detected", {})
	, invalidPsgDirectionsSetting(commandController,
		"invalid_psg_directions_callback",
		"Tcl proc called when the MSX program has set invalid PSG port directions",
		{})
	, resampleSetting(commandController, "resampler", "Resample algorithm",
#if PLATFORM_DINGUX
		// For Dingux, LQ is good compromise between quality and performance
		ResampledSoundDevice::RESAMPLE_LQ,
#elif PLATFORM_ANDROID
		// For Android, BLIP is good compromise between quality and performance
		ResampledSoundDevice::RESAMPLE_BLIP,
#else
		// For other platforms, default setting may be changed in future
		ResampledSoundDevice::RESAMPLE_BLIP,
#endif
		EnumSetting<ResampledSoundDevice::ResampleType>::Map{
			{"hq",   ResampledSoundDevice::RESAMPLE_HQ},
			{"fast", ResampledSoundDevice::RESAMPLE_LQ},
			{"blip", ResampledSoundDevice::RESAMPLE_BLIP}})
	, throttleManager(commandController)
{
	for (auto i : xrange(SDL_NumJoysticks())) {
		std::string name = strCat("joystick", i + 1, "_deadzone");
		deadzoneSettings.emplace_back(std::make_unique<IntegerSetting>(
			commandController, name,
			"size (as a percentage) of the dead center zone",
			25, 0, 100));
	}

	getPowerSetting().attach(*this);
}

GlobalSettings::~GlobalSettings()
{
	getPowerSetting().detach(*this);
	commandController.getSettingsConfig().setSaveSettings(
		autoSaveSetting.getBoolean());
}

// Observer<Setting>
void GlobalSettings::update(const Setting& setting)
{
	if (&setting == &getPowerSetting()) { // either on or off
		// automatically unpause after a power off/on cycle
		// this solved a bug, but apart from that this behaviour also
		// makes more sense
		getPauseSetting().setBoolean(false);
	}
}

} // namespace openmsx
