// $Id$

#include "MSXDevice.hh"
#include "xmlx.hh"


namespace openmsx {

MSXDevice::MSXDevice(const XMLElement& config, const EmuTime& /*time*/)
	: deviceConfig(config)
{
}

MSXDevice::~MSXDevice()
{
}

void MSXDevice::reset(const EmuTime& /*time*/)
{
}

void MSXDevice::reInit(const EmuTime& time)
{
	reset(time);
}

const string& MSXDevice::getName() const
{
	return deviceConfig.getAttribute("id");
}

} // namespace openmsx
