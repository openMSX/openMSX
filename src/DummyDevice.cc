#include "DummyDevice.hh"
#include "unreachable.hh"

namespace openmsx {

DummyDevice::DummyDevice(const DeviceConfig& config)
	: MSXDevice(config)
{
}

void DummyDevice::reset(EmuTime /*time*/)
{
	UNREACHABLE;
}

void DummyDevice::getNameList(TclObject& /*result*/) const
{
	// keep empty list
}

} // namespace openmsx
