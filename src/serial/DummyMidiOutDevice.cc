// $Id$

#include "DummyMidiOutDevice.hh"


void DummyMidiOutDevice::recvByte(byte value, const EmuTime& time)
{
	// ignore
	// PRT_DEBUG("Midi out " << std::hex << (int)value << std::dec);
}

void DummyMidiOutDevice::plug(Connector* connector, const EmuTime& time)
{
}

void DummyMidiOutDevice::unplug(const EmuTime& time)
{
}
