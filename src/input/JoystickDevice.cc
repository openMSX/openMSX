// $Id$

#include "JoystickDevice.hh"

using std::string;

namespace openmsx {

const string &JoystickDevice::getClass() const
{
	static const string className("Joystick Port");
	return className;
}

} // namespace openmsx
