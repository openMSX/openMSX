// $Id$

#include "MidiInDevice.hh"


const string& MidiInDevice::getClass() const
{
	static const string className("Midi In Port");
	return className;
}
