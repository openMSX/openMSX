// $Id$
 
#include <iostream>
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"


MSXDevice::MSXDevice(MSXConfig::Device *config, const EmuTime &time)
{
	PRT_DEBUG("instantiating an MSXDevice object..");
	deviceConfig=config;
	deviceName=&config->getId();
	PRT_DEBUG(".." << getName());
}

MSXDevice::MSXDevice()
{
	PRT_DEBUG("instantiating an MSXDevice object (Dummy)");
	deviceConfig = NULL;
}

MSXDevice::~MSXDevice()
{
	//PRT_DEBUG("Destructing an MSXDevice object");
}

void MSXDevice::reset(const EmuTime &time)
{
	PRT_DEBUG ("Resetting " << getName());
	isIRQset = false;
}

//
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
	if (deviceName != 0) {
		return *deviceName;
	} else {
		return defaultName;
	}
}
const std::string MSXDevice::defaultName = "no name";

//These are used for save/restoreState see note over
//savefile-structure
//bool MSXDevice::writeSaveStateHeader(std::ofstream &readstream )
//{
//	// TODO
//	return true;
//}
//bool MSXDevice::checkSaveStateHeader(std::string &devicestring)
//{
//	// TODO
//	return true;
//}


// Helper functions 

void MSXDevice::setInterrupt()
{
	if (isIRQset==false) {
		isIRQset = true;
		PRT_DEBUG ("Actually seting interupt line of " << getName());
		MSXMotherBoard::instance()->raiseIRQ();
	}
}
void MSXDevice::resetInterrupt()
{
	if (isIRQset==true) {
		isIRQset = false;
		PRT_DEBUG ("Actually resseting interupt line of " << getName());
		MSXMotherBoard::instance()->lowerIRQ();
	}
}
