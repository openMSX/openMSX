// $Id$

#include "MidiInDevice.hh"

using std::string;

namespace openmsx {

const string& MidiInDevice::getClass() const
{
	static const string className("midi in");
	return className;
}

} // namespace openmsx
