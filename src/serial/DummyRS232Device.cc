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

void DummyRS232Device::plug(Connector* connector, const EmuTime& time)
	throw()
{
}

void DummyRS232Device::unplug(const EmuTime& time)
{
}

void DummyRS232Device::recvByte(byte value, const EmuTime& time)
{
	// ignore
	// PRT_DEBUG("RS232 out " << hex << (int)value << dec);
}

} // namespace openmsx
