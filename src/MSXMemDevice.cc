// $Id$
 
#include "MSXCPUInterface.hh"
#include "MSXMemDevice.hh"
#include "MSXConfig.hh"


MSXMemDevice::MSXMemDevice(Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	registerSlots();
}

byte MSXMemDevice::readMem(word address, const EmuTime &time)
{
	PRT_DEBUG("MSXMemDevice: read from unmapped memory " << std::hex <<
	           (int)address << std::dec);
	return 255;
}

void MSXMemDevice::writeMem(word address, byte value, const EmuTime &time)
{
	PRT_DEBUG("MSXMemDevice: write to unmapped memory " << std::hex <<
	           (int)address << std::dec);
	// do nothing
}

void MSXMemDevice::registerSlots()
{
	// register in slot-structure
	if (deviceConfig == NULL) return;	// for DummyDevice
	std::list<Device::Slotted*>::const_iterator i;
	for (i=deviceConfig->slotted.begin(); i!=deviceConfig->slotted.end(); i++) {
		int ps = (*i)->getPS();
		int ss = (*i)->getSS();
		int page = (*i)->getPage();
		MSXCPUInterface::instance()->registerSlottedDevice(this, ps, ss, page);
	}
}

const byte* MSXMemDevice::getReadCacheLine(word start) const
{
	return NULL;	// uncacheable
}

byte* MSXMemDevice::getWriteCacheLine(word start) const
{
	return NULL;	// uncacheable
}
