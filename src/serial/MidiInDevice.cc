#include "MidiInDevice.hh"

namespace openmsx {

string_view MidiInDevice::getClass() const
{
	return "midi in";
}

} // namespace openmsx
