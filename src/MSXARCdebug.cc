// $Id$

#include "MSXARCdebug.hh"


MSXARCdebug::MSXARCdebug(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXARCdebug object");
}

MSXARCdebug::~MSXARCdebug()
{
	PRT_DEBUG("Destructing an MSXARCdebug object");
}

void MSXARCdebug::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
}

byte MSXARCdebug::readMem(word address, const EmuTime &time)
{
	PRT_DEBUG("MSXARCdebug::readMem(0x" << std::hex << (int)address << std::dec
		<< ").");
	return 255;
}

void MSXARCdebug::writeMem(word address, byte value, const EmuTime &time)
{
	PRT_DEBUG("MSXARCdebug::writeMem(0x" << std::hex << (int)address << std::dec
		<< ", value "<<(int)value<<").");
}

