// $Id$

#include <cassert>
#include "DummyDevice.hh"
#include "EmuTime.hh"
#include "Config.hh"
#include "FileContext.hh"

namespace openmsx {

DummyDevice::DummyDevice(Config* config, const EmuTime& time) 
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
	static Config config(deviceElem, dummyContext);
	static DummyDevice oneInstance(&config, EmuTime::zero);
	return oneInstance;
}


// Block usage of the following methods

void DummyDevice::reset(const EmuTime& time)
{
	assert(false);
}

} // namespace openmsx
