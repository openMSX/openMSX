// $Id$

#include "MSXMultiDevice.hh"
#include "MSXMotherBoard.hh"
#include "DeviceConfig.hh"
#include "unreachable.hh"

namespace openmsx {

static DeviceConfig getMultiConfig(MSXMotherBoard& motherboard)
{
	static XMLElement xml("Multi");
	return DeviceConfig(*motherboard.getMachineConfig(), xml);
}

// TODO take HardwareConfig parameter instead?
MSXMultiDevice::MSXMultiDevice(MSXMotherBoard& motherboard)
	: MSXDevice(getMultiConfig(motherboard), "Multi")
{
}

void MSXMultiDevice::reset(EmuTime::param /*time*/)
{
	UNREACHABLE;
}

void MSXMultiDevice::powerUp(EmuTime::param /*time*/)
{
	UNREACHABLE;
}

void MSXMultiDevice::powerDown(EmuTime::param /*time*/)
{
	UNREACHABLE;
}

} // namespace openmsx
