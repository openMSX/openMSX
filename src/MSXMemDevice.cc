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
	if (deviceConfig == NULL) {
		// DummyDevice
		return;
	}
	
	assert(!deviceConfig->slotted.empty());
	int ps = deviceConfig->slotted.front()->getPS();
	int ss = deviceConfig->slotted.front()->getSS();
	int pages = 0;
	std::list<Device::Slotted*>::const_iterator it;
	for (it  = deviceConfig->slotted.begin();
	     it != deviceConfig->slotted.end();
	     it++) {
		assert((*it)->getPS() == ps);
		assert((*it)->getSS() == ss);
		pages |= 1 << ((*it)->getPage());
	}
	if (ps >= 0) {
		// slot specified
		MSXCPUInterface::instance()->
			registerSlottedDevice(this, ps, ss, pages);
	} else {
		// take any free slot
		MSXCPUInterface::instance()->
			registerSlottedDevice(this, pages);
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
