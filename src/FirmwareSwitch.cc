// $Id$

#include "FirmwareSwitch.hh"
#include "BooleanSetting.hh"

namespace openmsx {

FirmwareSwitch::FirmwareSwitch()
	: setting(new BooleanSetting("firmwareswitch",
	          "This setting controls the firmware switch",
	          false))
{
}

bool FirmwareSwitch::getStatus() const
{
	return setting->getValue();
}

} // namespace openmsx
