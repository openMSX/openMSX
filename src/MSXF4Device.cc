// $Id$

#include "MSXF4Device.hh"


MSXF4Device::MSXF4Device(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	inverted = config->hasParameter("inverted")
		&& config->getParameterAsBool("inverted");
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
	return inverted ? status ^ 0xFF : status;
}

void MSXF4Device::writeIO(byte port, byte value, const EmuTime &time)
{
	status = (status & 0x20) | (value & 0xa0);
}
