// $Id$

#include "MSXF4Device.hh"
#include "MSXConfig.hh"


MSXF4Device::MSXF4Device(Device *config, const EmuTime &time)
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
	status = inverted ? 0xFF : 0x00;
}

byte MSXF4Device::readIO(byte port, const EmuTime &time)
{
	return status;
}

void MSXF4Device::writeIO(byte port, byte value, const EmuTime &time)
{
	if (inverted) {
		status = value | 0x7F;
	} else {
		status = (status & 0x20) | (value & 0xa0);
	}
}
