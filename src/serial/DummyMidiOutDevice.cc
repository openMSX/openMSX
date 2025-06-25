#include "DummyMidiOutDevice.hh"

namespace openmsx {

void DummyMidiOutDevice::recvByte(uint8_t /*value*/, EmuTime /*time*/)
{
	// ignore
}

std::string_view DummyMidiOutDevice::getDescription() const
{
	return {};
}

void DummyMidiOutDevice::plugHelper(
		Connector& /*connector*/, EmuTime /*time*/)
{
}

void DummyMidiOutDevice::unplugHelper(EmuTime /*time*/)
{
}

} // namespace openmsx
