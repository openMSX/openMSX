// $Id$

#include "MidiInDevice.hh"


namespace openmsx {

const string& MidiInDevice::getClass() const
{
	static const string className("midi in");
	return className;
}

} // namespace openmsx
