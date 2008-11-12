// $Id$

#include "DummyRS232Device.hh"

namespace openmsx {

void DummyRS232Device::signal(EmuTime::param /*time*/)
{
	// ignore
}

const std::string& DummyRS232Device::getDescription() const
{
	static const std::string EMPTY;
	return EMPTY;
}

void DummyRS232Device::plugHelper(Connector& /*connector*/,
                                  EmuTime::param /*time*/)
{
}

void DummyRS232Device::unplugHelper(EmuTime::param /*time*/)
{
}

void DummyRS232Device::recvByte(byte /*value*/, EmuTime::param /*time*/)
{
	// ignore
}

} // namespace openmsx
