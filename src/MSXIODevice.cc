// $Id$
 
#include "MSXIODevice.hh"

MSXIODevice::MSXIODevice()
{
	// TODO registerIO();
}

byte MSXIODevice::readIO(byte port, const EmuTime &time)
{
#ifdef DEBUG
	PRT_DEBUG("MSXIODevice::readIO (0x" << std::hex << (int)port << std::dec
		<< ") : No device implementation.");
#endif
	return 255;
}
void MSXIODevice::writeIO(byte port, byte value, const EmuTime &time)
{
#ifdef DEBUG
	PRT_DEBUG("MSXIODevice::writeIO(port 0x" << std::hex << (int)port << std::dec
		<<",value "<<(int)value<<") : No device implementatation."<<port);
#endif
	// do nothing
}
