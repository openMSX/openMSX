// $Id$

#include "DummyMidiInDevice.hh"

namespace openmsx {

void DummyMidiInDevice::signal(const EmuTime& time)
{
	// ignore
}

const string& DummyMidiInDevice::getDescription() const
{
	static const string EMPTY;
	return EMPTY;
}

void DummyMidiInDevice::plug(Connector* connector, const EmuTime& time)
	throw()
{
}

void DummyMidiInDevice::unplug(const EmuTime& time)
{
}

} // namespace openmsx
