// $Id$

#include "MidiInDevice.hh"


const string& MidiInDevice::getClass() const
{
	static const string className("midi in");
	return className;
}
