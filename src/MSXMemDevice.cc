// $Id$
 
#include "MSXMotherBoard.hh"
#include "MSXMemDevice.hh"


MSXMemDevice::MSXMemDevice(MSXConfig::Device *config, const EmuTime &time)
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
	std::list<MSXConfig::Device::Slotted*>::const_iterator i;
	for (i=deviceConfig->slotted.begin(); i!=deviceConfig->slotted.end(); i++) {
		int ps = (*i)->getPS();
		int ss = (*i)->getSS();
		int page = (*i)->getPage();
		MSXMotherBoard::instance()->registerSlottedDevice(this, ps, ss, page);
	}
}

byte* MSXMemDevice::getReadCacheLine(word start)
{
	return NULL;	// uncacheable
}

byte* MSXMemDevice::getWriteCacheLine(word start)
{
	return NULL;	// uncacheable
}
