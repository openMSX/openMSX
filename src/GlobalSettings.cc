// $Id$

#include "GlobalSettings.hh"

namespace openmsx {

GlobalSettings::GlobalSettings()
        : pauseSetting("pause", "pauses the emulation", false)
	, powerSetting("power", "turn power on/off", false)
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

} // namespace openmsx
