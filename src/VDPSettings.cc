// $Id$

#include "VDPSettings.hh"


VDPSettings::VDPSettings()
{
	std::map<const std::string, bool> cmdMap;
	cmdMap["real"]   = false;
	cmdMap["broken"] = true;
	cmdTiming = new EnumSetting<bool>(
		"cmdtiming", "VDP command timing", false, cmdMap);
}

VDPSettings::~VDPSettings()
{
	delete cmdTiming;
}
