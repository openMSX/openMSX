// $Id$

#include "JoystickDevice.hh"


const std::string &JoystickDevice::getClass()
{
	static const std::string className("Joystick Port");
	return className;
}
