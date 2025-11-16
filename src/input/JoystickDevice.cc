#include "JoystickDevice.hh"

namespace openmsx {

zstring_view JoystickDevice::getClass() const
{
	return "Joystick Port";
}

} // namespace openmsx
