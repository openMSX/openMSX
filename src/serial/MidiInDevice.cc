#include "MidiInDevice.hh"

namespace openmsx {

zstring_view MidiInDevice::getClass() const
{
	return "midi in";
}

} // namespace openmsx
