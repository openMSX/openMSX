#include "DummyRS232Device.hh"

namespace openmsx {

void DummyRS232Device::signal(EmuTime::param /*time*/)
{
	// ignore
}

std::string_view DummyRS232Device::getDescription() const
{
	return {};
}

void DummyRS232Device::plugHelper(Connector& /*connector*/,
                                  EmuTime::param /*time*/)
{
}

void DummyRS232Device::unplugHelper(EmuTime::param /*time*/)
{
}

void DummyRS232Device::recvByte(uint8_t /*value*/, EmuTime::param /*time*/)
{
	// ignore
}

} // namespace openmsx
