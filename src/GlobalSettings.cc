// $Id$

#include "GlobalSettings.hh"
#include "XMLElement.hh"
#include "FileContext.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "StringSetting.hh"
#include "SettingsConfig.hh"

namespace openmsx {

GlobalSettings::GlobalSettings()
{
	speedSetting.reset(new IntegerSetting("speed",
	       "controls the emulation speed: higher is faster, 100 is normal",
	       100, 1, 1000000));
	throttleSetting.reset(new BooleanSetting("throttle",
	       "controls speed throttling", true));
	pauseSetting.reset(new BooleanSetting("pause", "pauses the emulation",
	       false, Setting::DONT_SAVE));
	powerSetting.reset(new BooleanSetting("power", "turn power on/off",
	        false, Setting::DONT_SAVE));
	autoSaveSetting.reset(new BooleanSetting("save_settings_on_exit",
	        "automatically save settings when openMSX exits", true));
	consoleSetting.reset(new BooleanSetting("console",
	        "turns console display on/off", false, Setting::DONT_SAVE));
	userDirSetting.reset(new StringSetting("user_directories",
	        "list of user directories", ""));
}

GlobalSettings::~GlobalSettings()
{
	SettingsConfig::instance().setSaveSettings(autoSaveSetting->getValue());
}

GlobalSettings& GlobalSettings::instance()
{
	static GlobalSettings oneInstance;
	return oneInstance;
}

IntegerSetting& GlobalSettings::getSpeedSetting()
{
	return *speedSetting.get();
}

BooleanSetting& GlobalSettings::getThrottleSetting()
{
	return *throttleSetting.get();
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

StringSetting& GlobalSettings::getUserDirSetting()
{
	return *userDirSetting.get();
}

XMLElement& GlobalSettings::getMediaConfig()
{
	if (!mediaConfig.get()) {
		mediaConfig.reset(new XMLElement("media"));
		mediaConfig->setFileContext(
			std::auto_ptr<FileContext>(new UserFileContext()));
	}
	return *mediaConfig;
}

} // namespace openmsx
