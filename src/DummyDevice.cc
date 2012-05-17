// $Id$

#include "DummyDevice.hh"
#include "unreachable.hh"

namespace openmsx {

DummyDevice::DummyDevice(MSXMotherBoard& motherBoard, const DeviceConfig& config)
	: MSXDevice(motherBoard, config)
{
}

void DummyDevice::reset(EmuTime::param /*time*/)
{
	UNREACHABLE;
}

} // namespace openmsx
