// $Id$

#include "FirmwareSwitch.hh"

// TODO rename this class to FirmwareSwitch

namespace openmsx {

FirmwareSwitch::FirmwareSwitch()
	: setting("firmwareswitch",
	          "This setting controls the firmware switch",
	          false)
{
}

bool FirmwareSwitch::getStatus() const
{
	return setting.getValue();
}

} // namespace openmsx
