// $Id$

#include "MSXDevice.hh"


MSXDevice::MSXDevice(MSXConfig::Device *config, const EmuTime &time)
{
	deviceConfig = config;
	PRT_DEBUG("instantiating an MSXDevice object: " << getName());
}

MSXDevice::~MSXDevice()
{
	//PRT_DEBUG("Destructing an MSXDevice object");
}

void MSXDevice::reset(const EmuTime &time)
{
}


const std::string &MSXDevice::getName()
{
	static const std::string defaultName = "empty";

	if (deviceConfig) {
		return deviceConfig->getId();
	} else {
		// TODO only for DummyDevice -> fix DummyDevice
		return defaultName;
	}
}
