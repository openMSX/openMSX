// $Id$

#include "DummyRS232Device.hh"

namespace openmsx {

void DummyRS232Device::signal(const EmuTime& time)
{
	// ignore
}

const string& DummyRS232Device::getDescription() const
{
	static const string EMPTY;
	return EMPTY;
}

void DummyRS232Device::plugHelper(Connector* connector, const EmuTime& time)
{
}

void DummyRS232Device::unplugHelper(const EmuTime& time)
{
}

void DummyRS232Device::recvByte(byte value, const EmuTime& time)
{
	// ignore
	// PRT_DEBUG("RS232 out " << hex << (int)value << dec);
}

} // namespace openmsx
