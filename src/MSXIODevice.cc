// $Id$

#include "MSXIODevice.hh"


MSXIODevice::MSXIODevice(Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	// TODO registerIO();
}

byte MSXIODevice::readIO(byte port, const EmuTime &time)
{
	PRT_DEBUG("MSXIODevice::readIO (0x" << std::hex << (int)port << std::dec
		<< ") : No device implementation.");
	return 255;
}
void MSXIODevice::writeIO(byte port, byte value, const EmuTime &time)
{
	PRT_DEBUG("MSXIODevice::writeIO(port 0x" << std::hex << (int)port << std::dec
		<<",value "<<(int)value<<") : No device implementation.");
	// do nothing
}
