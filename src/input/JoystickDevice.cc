// $Id$

#include "JoystickDevice.hh"


namespace openmsx {

const string &JoystickDevice::getClass() const
{
	static const string className("Joystick Port");
	return className;
}

} // namespace openmsx
