// $Id$

#include "VDPSettings.hh"


VDPSettings::VDPSettings()
{
	limitSprites = new BooleanSetting(
		"limitsprites", "limit number of sprites per line "
		"(on for realism, off to reduce sprite flashing)", true);

	map<const string, bool> cmdMap;
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
