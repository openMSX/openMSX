// $Id$

#include "DummyMidiInDevice.hh"

namespace openmsx {

void DummyMidiInDevice::signal(const EmuTime& time)
{
	// ignore
}

void DummyMidiInDevice::plug(Connector* connector, const EmuTime& time)
{
}

void DummyMidiInDevice::unplug(const EmuTime& time)
{
}

} // namespace openmsx
