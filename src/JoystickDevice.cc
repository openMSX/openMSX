// $Id$

#include "JoystickDevice.hh"


const std::string &JoystickDevice::getClass()
{
	return className;
}

const std::string JoystickDevice::className("Joystick Port");
