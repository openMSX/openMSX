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
	static DummyDevice* oneInstance = NULL;
	if (oneInstance == NULL) {
		EmuTime dummy;
		oneInstance = new DummyDevice(NULL, dummy); //TODO make a MSXConfig 
	}
	return oneInstance;
}


// Block usage of the following methods

void DummyDevice::init()
{
	assert(false);
}

void DummyDevice::reset(const EmuTime &time)
{
	assert(false);
}
