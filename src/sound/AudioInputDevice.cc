#include "AudioInputDevice.hh"

using std::string;

namespace openmsx {

string_ref AudioInputDevice::getClass() const
{
	return "Audio Input Port";
}

} // namespace openmsx
