// $Id$

#include "VDPSettings.hh"

namespace openmsx {

VDPSettings::VDPSettings()
{
	limitSprites.reset(new BooleanSetting(
		"limitsprites", "limit number of sprites per line "
		"(on for realism, off to reduce sprite flashing)", true));

	EnumSetting<bool>::Map cmdMap;
	cmdMap["real"]   = false;
	cmdMap["broken"] = true;
	cmdTiming.reset(new EnumSetting<bool>(
		"cmdtiming", "VDP command timing", false, cmdMap));
}

VDPSettings::~VDPSettings()
{
}

VDPSettings& VDPSettings::instance()
{
	static VDPSettings oneInstance;
	return oneInstance;
}

} // namespace openmsx
