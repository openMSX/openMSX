#include "DummyMidiOutDevice.hh"

namespace openmsx {

void DummyMidiOutDevice::recvByte(uint8_t /*value*/, EmuTime::param /*time*/)
{
	// ignore
}

std::string_view DummyMidiOutDevice::getDescription() const
{
	return {};
}

void DummyMidiOutDevice::plugHelper(
		Connector& /*connector*/, EmuTime::param /*time*/)
{
}

void DummyMidiOutDevice::unplugHelper(EmuTime::param /*time*/)
{
}

} // namespace openmsx
