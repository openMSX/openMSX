// $Id$

#include "VDPSettings.hh"


namespace openmsx {

VDPSettings::VDPSettings()
{
	limitSprites = new BooleanSetting(
		"limitsprites", "limit number of sprites per line "
		"(on for realism, off to reduce sprite flashing)", true);

	EnumSetting<bool>::Map cmdMap;
	cmdMap["real"]   = false;
	cmdMap["broken"] = true;
	cmdTiming = new EnumSetting<bool>(
		"cmdtiming", "VDP command timing", false, cmdMap);
}

VDPSettings::~VDPSettings()
{
	delete limitSprites;
	delete cmdTiming;
}

VDPSettings& VDPSettings::instance()
{
	static VDPSettings oneInstance;
	return oneInstance;
}

} // namespace openmsx
