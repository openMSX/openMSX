// $Id$

#include "MSXF4Device.hh"
#include "MSXCPUInterface.hh"


MSXF4Device::MSXF4Device(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	MSXCPUInterface::instance()->register_IO_In (0xF4,this);
	MSXCPUInterface::instance()->register_IO_Out(0xF4,this);
	reset(time);
}

MSXF4Device::~MSXF4Device()
{
}
 
void MSXF4Device::reset(const EmuTime &time)
{
	status = 0;
}

byte MSXF4Device::readIO(byte port, const EmuTime &time)
{
	return status;
}

void MSXF4Device::writeIO(byte port, byte value, const EmuTime &time)
{
	status = ((status & 0x20) | (value & 0xa0));
}
