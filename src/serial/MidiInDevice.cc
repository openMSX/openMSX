#include "MidiInDevice.hh"

namespace openmsx {

std::string_view MidiInDevice::getClass() const
{
	return "midi in";
}

} // namespace openmsx
