#include "JoystickDevice.hh"

namespace openmsx {

string_view JoystickDevice::getClass() const
{
	return "Joystick Port";
}

} // namespace openmsx
