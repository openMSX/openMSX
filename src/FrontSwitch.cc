// $Id$

#include "FrontSwitch.hh"

// TODO rename this class to FirmwareSwitch

namespace openmsx {

FrontSwitch::FrontSwitch()
	: setting("firmwareswitch",
	          "This setting controls the firmware switch",
	          false)
{
}

bool FrontSwitch::getStatus() const
{
	return setting.getValue();
}

} // namespace openmsx
