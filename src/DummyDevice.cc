// $Id$

#include "DummyDevice.hh"
#include <cassert>


DummyDevice::DummyDevice(MSXConfig::Device *config, const EmuTime &time) 
	: MSXDevice(config, time), MSXIODevice(config, time), MSXMemDevice(config, time)
{
	PRT_DEBUG ("Instantiating dummy device");
}

DummyDevice::~DummyDevice()
{
	PRT_DEBUG ("Destroying dummy device");
}

DummyDevice* DummyDevice::instance()
{
	if (oneInstance == NULL ) {
		EmuTime dummy;
		oneInstance = new DummyDevice(NULL, dummy); //TODO make a MSXConfig 
	}
	return oneInstance;
}
DummyDevice *DummyDevice::oneInstance = NULL;


// Block usage of the following methods

void DummyDevice::init()
{
	assert(false);
}

void DummyDevice::reset(const EmuTime &time)
{
	assert(false);
}

void DummyDevice::saveState(std::ofstream &writestream)
{
	assert(false);
}

void DummyDevice::restoreState(std::string &devicestring, std::ifstream &readstream)
{
	assert(false);
}
