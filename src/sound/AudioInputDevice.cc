#include "AudioInputDevice.hh"

namespace openmsx {

std::string_view AudioInputDevice::getClass() const
{
	return "Audio Input Port";
}

} // namespace openmsx
