// $Id$

#include "JoystickDevice.hh"


const std::string &JoystickDevice::getClass() const
{
	static const std::string className("Joystick Port");
	return className;
}
