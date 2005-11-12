// $Id$

#include "GlobalSettings.hh"
#include "XMLElement.hh"
#include "FileContext.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "StringSetting.hh"
#include "SettingsConfig.hh"
#include "ThrottleManager.hh"

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
	consoleSetting.reset(new BooleanSetting(commandController, "console",
	        "turns console display on/off", false, Setting::DONT_SAVE));
	userDirSetting.reset(new StringSetting(commandController,
	        "user_directories", "list of user directories", ""));
	throttleManager.reset(new ThrottleManager(commandController));
}

GlobalSettings::~GlobalSettings()
{
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

BooleanSetting& GlobalSettings::getConsoleSetting()
{
	return *consoleSetting.get();
}

ThrottleManager& GlobalSettings::getThrottleManager()
{
	return *throttleManager.get();
}

StringSetting& GlobalSettings::getUserDirSetting()
{
	return *userDirSetting.get();
}

XMLElement& GlobalSettings::getMediaConfig()
{
	if (!mediaConfig.get()) {
		mediaConfig.reset(new XMLElement("media"));
		mediaConfig->setFileContext(
			std::auto_ptr<FileContext>(
				new UserFileContext(commandController)));
	}
	return *mediaConfig;
}

} // namespace openmsx
