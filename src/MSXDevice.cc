// $Id$

#include "MSXDevice.hh"
#include "Config.hh"


namespace openmsx {

MSXDevice::MSXDevice(Config* config, const EmuTime& time)
	: deviceConfig(config)
{
}

MSXDevice::~MSXDevice()
{
}

void MSXDevice::reset(const EmuTime& time)
{
}

void MSXDevice::reInit(const EmuTime& time)
{
	reset(time);
}

const string& MSXDevice::getName() const
{
	return deviceConfig->getId();
}

} // namespace openmsx
