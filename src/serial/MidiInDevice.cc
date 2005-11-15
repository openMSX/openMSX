// $Id$

#include "MidiInDevice.hh"

namespace openmsx {

const std::string& MidiInDevice::getClass() const
{
	static const std::string className("midi in");
	return className;
}

} // namespace openmsx
