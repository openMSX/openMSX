#include "DummyRS232Device.hh"

namespace openmsx {

void DummyRS232Device::signal(EmuTime /*time*/)
{
	// ignore
}

zstring_view DummyRS232Device::getDescription() const
{
	return {};
}

void DummyRS232Device::plugHelper(Connector& /*connector*/,
                                  EmuTime /*time*/)
{
}

void DummyRS232Device::unplugHelper(EmuTime /*time*/)
{
}

void DummyRS232Device::recvByte(uint8_t /*value*/, EmuTime /*time*/)
{
	// ignore
}

} // namespace openmsx
