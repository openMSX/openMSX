// $Id$

#include <cassert>
#include "DummyDevice.hh"
#include "EmuTime.hh"
#include "Device.hh"

namespace openmsx {

DummyDevice::DummyDevice(Device* config, const EmuTime& time) 
	: MSXDevice(config, time), MSXIODevice(config, time),
	  MSXMemDevice(config, time)
{
}

DummyDevice::~DummyDevice()
{
}

DummyDevice& DummyDevice::instance()
{
	static Device deviceConfig("Dummy", "empty");
	static DummyDevice oneInstance(&deviceConfig, EmuTime::zero);
	return oneInstance;
}


// Block usage of the following methods

void DummyDevice::reset(const EmuTime& time)
{
	assert(false);
}

} // namespace openmsx
