// $Id$

#include "DummyMidiOutDevice.hh"

namespace openmsx {

void DummyMidiOutDevice::recvByte(byte /*value*/, const EmuTime& /*time*/)
{
	// ignore
}

const std::string& DummyMidiOutDevice::getDescription() const
{
	static const std::string EMPTY;
	return EMPTY;
}

void DummyMidiOutDevice::plugHelper(
		Connector& /*connector*/, const EmuTime& /*time*/)
{
}

void DummyMidiOutDevice::unplugHelper(const EmuTime& /*time*/)
{
}

} // namespace openmsx
