// $Id$
 
#include <iostream>
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"


MSXDevice::MSXDevice()
{
	PRT_DEBUG("instantiating an MSXDevice object");
}

MSXDevice::~MSXDevice()
{
	PRT_DEBUG("destructing an MSXDevice object");
}

void MSXDevice::setConfigDevice(MSXConfig::Device *config)
{
	deviceConfig=config;
	deviceName=&config->getId();
}

// interaction with CPU
byte MSXDevice::readMem(word address, Emutime &time)
{
	return 255;
}
void MSXDevice::writeMem(word address, byte value, Emutime &time)
{
	// do nothing
}
byte MSXDevice:: readIO(byte port, Emutime &time)
{
	return 255;
}
void MSXDevice::writeIO(byte port, byte value, Emutime &time)
{
	// do nothing
}
void MSXDevice::executeUntilEmuTime(const Emutime &time)
{
	PRT_DEBUG ("Empty executeUntilEmuTime function");
}


void MSXDevice::init()
{
	PRT_DEBUG ("Initializing " << getName());
}
void MSXDevice::start()
{
	PRT_DEBUG ("Starting " << getName());
	// default implementation same as reset
	reset();
}
void MSXDevice::stop()
{
	PRT_DEBUG ("Stopping " << getName());
}
void MSXDevice::reset()
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
//const std::string MSXDevice::defaultName = "no name";


//These are used for save/restoreState see note over
//savefile-structure
bool MSXDevice::writeSaveStateHeader(std::ofstream &readstream )
{
	// TODO
	return true;
}
bool MSXDevice::checkSaveStateHeader(std::string &devicestring)
{
	// TODO
	return true;
}


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
