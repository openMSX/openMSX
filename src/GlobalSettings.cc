// $Id$

#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "StringSetting.hh"
#include "SettingsConfig.hh"
#include "CommandController.hh"
#include "ThrottleManager.hh"
#include "EnumSetting.hh"

namespace openmsx {

GlobalSettings::GlobalSettings(CommandController& commandController_)
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
	userDirSetting.reset(new StringSetting(commandController,
	        "user_directories", "list of user directories", ""));
	pauseOnLostFocusSetting.reset(new BooleanSetting(commandController, "pause_on_lost_focus",
	       "pause emulation when the openMSX window loses focus", false));
	umrCallBackSetting.reset(new StringSetting(commandController,
	        "umr_callback", "Tcl proc to call when an UMR is detected", ""));
	EnumSetting<DirAsDSK::SyncMode>::Map syncDirAsDSKMap;
	syncDirAsDSKMap["read_only"] = DirAsDSK::SYNC_READONLY;
	syncDirAsDSKMap["cached_write"] = DirAsDSK::SYNC_CACHEDWRITE;
	syncDirAsDSKMap["nodelete"] = DirAsDSK::SYNC_NODELETE;
	syncDirAsDSKMap["full"] = DirAsDSK::SYNC_FULL;
	syncDirAsDSKSetting.reset(new EnumSetting<DirAsDSK::SyncMode>(
		commandController, "DirAsDSKmode",
		"type of syncronisation between host directory and dir-as-dsk diskimage",
		DirAsDSK::SYNC_FULL, syncDirAsDSKMap));
	EnumSetting<DirAsDSK::BootSectorType>::Map bootsectorMap;
	bootsectorMap["DOS1"] = DirAsDSK::BOOTSECTOR_DOS1;
	bootsectorMap["DOS2"] = DirAsDSK::BOOTSECTOR_DOS2;
	bootSectorSetting.reset(new EnumSetting<DirAsDSK::BootSectorType>(
		commandController, "bootsector", "boot sector type for dir-as-dsk",
		DirAsDSK::BOOTSECTOR_DOS2, bootsectorMap));
	EnumSetting<Resample::ResampleType>::Map resampleMap;
	resampleMap["hq"]   = Resample::RESAMPLE_HQ;
	resampleMap["fast"] = Resample::RESAMPLE_LQ;
	resampleMap["blip"] = Resample::RESAMPLE_BLIP;
	resampleSetting.reset(new EnumSetting<Resample::ResampleType>(
		commandController, "resampler", "Resample algorithm",
		Resample::RESAMPLE_BLIP, resampleMap));

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

StringSetting& GlobalSettings::getUserDirSetting()
{
	return *userDirSetting.get();
}

BooleanSetting& GlobalSettings::getPauseOnLostFocusSetting()
{
	return *pauseOnLostFocusSetting.get();
}

EnumSetting<DirAsDSK::SyncMode>& GlobalSettings::getSyncDirAsDSKSetting()
{
	return *syncDirAsDSKSetting.get();
}

EnumSetting<DirAsDSK::BootSectorType>& GlobalSettings::getBootSectorSetting()
{
	return *bootSectorSetting.get();
}

EnumSetting<Resample::ResampleType>& GlobalSettings::getResampleSetting()
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
