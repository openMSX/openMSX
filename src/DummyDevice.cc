// $Id$

#include <cassert>
#include "DummyDevice.hh"
#include "EmuTime.hh"
#include "Device.hh"
#include "FileContext.hh"

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
	XMLElement deviceElem("Dummy");
	XMLElement* typeElem = new XMLElement("type", "empty");
	deviceElem.addChild(typeElem);
	SystemFileContext dummyContext;
	static Device device(deviceElem, dummyContext);
	static DummyDevice oneInstance(&device, EmuTime::zero);
	return oneInstance;
}


// Block usage of the following methods

void DummyDevice::reset(const EmuTime& time)
{
	assert(false);
}

} // namespace openmsx
