// $Id$
 
#include "MSXMotherBoard.hh"
#include "MSXMemDevice.hh"


MSXMemDevice::MSXMemDevice()
{
	registerSlots();
}

byte MSXMemDevice::readMem(word address, const EmuTime &time)
{
	return 255;
}

void MSXMemDevice::writeMem(word address, byte value, const EmuTime &time)
{
	// do nothing
}

void MSXMemDevice::registerSlots()
{
	// register in slot-structure
	if (deviceConfig==NULL) return;	// for DummyDevice
	std::list<MSXConfig::Device::Slotted*>::const_iterator i;
	for (i=deviceConfig->slotted.begin(); i!=deviceConfig->slotted.end(); i++) {
		int ps=(*i)->getPS();
		int ss=(*i)->getSS();
		int page=(*i)->getPage();
		MSXMotherBoard::instance()->registerSlottedDevice(this,ps,ss,page);
	}
}
