// $Id$

#include "DummyDevice.hh"
#include "EmuTime.hh"
#include <cassert>


namespace openmsx {

DummyDevice::DummyDevice(Device *config, const EmuTime &time) 
	: MSXDevice(config, time), MSXIODevice(config, time),
	  MSXMemDevice(config, time)
{
}

DummyDevice::~DummyDevice()
{
}

DummyDevice* DummyDevice::instance()
{
	static DummyDevice oneInstance(NULL, EmuTime::zero);
	return &oneInstance;
}


// Block usage of the following methods

void DummyDevice::reset(const EmuTime &time)
{
	assert(false);
}

} // namespace openmsx
