// $Id$

#include "GlobalSettings.hh"
#include "xmlx.hh"
#include "FileContext.hh"

namespace openmsx {

GlobalSettings::GlobalSettings()
	: pauseSetting("pause", "pauses the emulation", false, DONT_SAVE_SETTING)
	, powerSetting("power", "turn power on/off", false, DONT_SAVE_SETTING)
	, autoSaveSetting("save_settings_on_exit",
	                  "automatically save settings when openMSX exits",
	                  true)
	, consoleSetting("console", "turns console display on/off", false,
	                 DONT_SAVE_SETTING)
	, userDirSetting("user_directories", "list of user directories", "")
{
}

GlobalSettings& GlobalSettings::instance()
{
	static GlobalSettings oneInstance;
	return oneInstance;
}

BooleanSetting& GlobalSettings::getPauseSetting()
{
	return pauseSetting;
}

BooleanSetting& GlobalSettings::getPowerSetting()
{
	return powerSetting;
}

BooleanSetting& GlobalSettings::getAutoSaveSetting()
{
	return autoSaveSetting;
}

BooleanSetting& GlobalSettings::getConsoleSetting()
{
	return consoleSetting;
}

StringSetting& GlobalSettings::getUserDirSetting()
{
	return userDirSetting;
}

XMLElement& GlobalSettings::getMediaConfig()
{
	if (!mediaConfig.get()) {
		mediaConfig.reset(new XMLElement("media"));
		mediaConfig->setFileContext(
			auto_ptr<FileContext>(new UserFileContext()));
	}
	return *mediaConfig;
}

} // namespace openmsx
