// $Id$

#include "DummyMidiOutDevice.hh"


namespace openmsx {

void DummyMidiOutDevice::recvByte(byte value, const EmuTime& time)
{
	// ignore
	// PRT_DEBUG("Midi out " << hex << (int)value << dec);
}

void DummyMidiOutDevice::plug(Connector* connector, const EmuTime& time)
{
}

void DummyMidiOutDevice::unplug(const EmuTime& time)
{
}

} // namespace openmsx
