#include "JoystickDevice.hh"

namespace openmsx {

std::string_view JoystickDevice::getClass() const
{
	return "Joystick Port";
}

} // namespace openmsx
