// $Id$

#include "MSXDevice.hh"
#include "MSXConfig.hh"


namespace openmsx {

MSXDevice::MSXDevice(Device *config, const EmuTime &time)
	: deviceConfig(config)
{
	//PRT_DEBUG("Instantiating MSXDevice: " << getName());
}

MSXDevice::~MSXDevice()
{
	//PRT_DEBUG("Destructing MSXDevice: " << getName());
}

void MSXDevice::reset(const EmuTime &time)
{
}


const string &MSXDevice::getName() const
{
	static const string defaultName = "empty";

	if (deviceConfig) {
		return deviceConfig->getId();
	} else {
		// TODO only for DummyDevice -> fix DummyDevice
		return defaultName;
	}
}

} // namespace openmsx
