// $Id$

#include "MSXDevice.hh"


MSXDevice::MSXDevice(MSXConfig::Device *config, const EmuTime &time)
{
	PRT_DEBUG("instantiating an MSXDevice object..");
	deviceConfig = config;
	PRT_DEBUG(".." << getName());
}

MSXDevice::~MSXDevice()
{
	//PRT_DEBUG("Destructing an MSXDevice object");
}

void MSXDevice::reset(const EmuTime &time)
{
	PRT_DEBUG ("Resetting " << getName());
}


void MSXDevice::saveState(std::ofstream &writestream)
{
	// default implementation:
	//   nothing needs to be saved
}
void MSXDevice::restoreState(std::string &devicestring, std::ifstream &readstream)
{
	// default implementation:
	//   nothing needs to be restored
}

const std::string &MSXDevice::getName()
{
	if (deviceConfig) {
		return deviceConfig->getId();
	} else {
		// TODO only for DummyDevice -> fix DummyDevice
		return defaultName;
	}
}
const std::string MSXDevice::defaultName = "empty";

