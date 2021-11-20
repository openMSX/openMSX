#include "GlobalSettings.hh"
#include "SettingsConfig.hh"
#include "GlobalCommandController.hh"
#include "strCat.hh"
#include "view.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <memory>
#include <SDL.h>

namespace openmsx {

GlobalSettings::GlobalSettings(GlobalCommandController& commandController_)
	: commandController(commandController_)
	, pauseSetting(commandController, "pause",
	       "pauses the emulation", false, Setting::DONT_SAVE)
	, powerSetting(commandController, "power",
	        "turn power on/off", false, Setting::DONT_SAVE)
	, autoSaveSetting(commandController, "save_settings_on_exit",
	        "automatically save settings when openMSX exits", true)
	, umrCallBackSetting(commandController, "umr_callback",
		"Tcl proc to call when an UMR is detected", {})
	, invalidPsgDirectionsSetting(commandController,
		"invalid_psg_directions_callback",
		"Tcl proc called when the MSX program has set invalid PSG port directions",
		{})
	, invalidPpiModeSetting(commandController,
		"invalid_ppi_mode_callback",
		"Tcl proc called when the MSX program has set an invalid PPI mode",
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
	, speedManager(commandController)
	, throttleManager(commandController)
{
	deadzoneSettings = to_vector(
		view::transform(xrange(SDL_NumJoysticks()), [&](auto i) {
			return std::make_unique<IntegerSetting>(
				commandController,
				tmpStrCat("joystick", i + 1, "_deadzone"),
				"size (as a percentage) of the dead center zone",
				25, 0, 100);
		}));
	getPowerSetting().attach(*this);
}

GlobalSettings::~GlobalSettings()
{
	getPowerSetting().detach(*this);
	commandController.getSettingsConfig().setSaveSettings(
		autoSaveSetting.getBoolean());
}

// Observer<Setting>
void GlobalSettings::update(const Setting& setting) noexcept
{
	if (&setting == &getPowerSetting()) { // either on or off
		// automatically unpause after a power off/on cycle
		// this solved a bug, but apart from that this behaviour also
		// makes more sense
		try {
			getPauseSetting().setBoolean(false);
		} catch(...) {
			// Ignore. E.g. can trigger when a Tcl trace on the
			// pause setting triggers errors in the Tcl script.
		}
	}
}

} // namespace openmsx
