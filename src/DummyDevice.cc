// $Id$

#include "DummyDevice.hh"
#include "unreachable.hh"

namespace openmsx {

DummyDevice::DummyDevice(const DeviceConfig& config)
	: MSXDevice(config)
{
}

void DummyDevice::reset(EmuTime::param /*time*/)
{
	UNREACHABLE;
}

} // namespace openmsx
