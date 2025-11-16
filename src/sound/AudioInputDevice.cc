#include "AudioInputDevice.hh"

namespace openmsx {

zstring_view AudioInputDevice::getClass() const
{
	return "Audio Input Port";
}

} // namespace openmsx
