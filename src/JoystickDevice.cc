// $Id$

#include "JoystickDevice.hh"


const string &JoystickDevice::getClass() const
{
	static const string className("Joystick Port");
	return className;
}
