// $Id$

#include "MSXIODevice.hh"


MSXIODevice::MSXIODevice(Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	// TODO registerIO();
}

MSXIODevice::~MSXIODevice()
{
}

byte MSXIODevice::readIO(byte port, const EmuTime &time)
{
	PRT_DEBUG("MSXIODevice::readIO (0x" << hex << (int)port << dec
		<< ") : No device implementation.");
	return 255;
}
void MSXIODevice::writeIO(byte port, byte value, const EmuTime &time)
{
	PRT_DEBUG("MSXIODevice::writeIO(port 0x" << hex << (int)port << dec
		<<",value "<<(int)value<<") : No device implementation.");
	// do nothing
}
