// $Id$

#include "MSXE6Timer.hh"
#include <cassert>


MSXE6Timer::MSXE6Timer(MSXConfig::Device *config) : MSXDevice(config), 
                                                    reference(255681)	// 1/14 * 3.58MHz
{
}

MSXE6Timer::~MSXE6Timer()
{
}
 
void MSXE6Timer::init()
{
	MSXDevice::init();
	MSXMotherBoard::instance()->register_IO_In (0xE6,this);
	MSXMotherBoard::instance()->register_IO_In (0xE7,this);
	MSXMotherBoard::instance()->register_IO_Out(0xE6,this);
}

void MSXE6Timer::reset()
{
	MSXDevice::reset();
	reference(0);
}

byte MSXE6Timer::readIO(byte port, Emutime &time)
{
	int counter = reference.getTicksTill(time);
	switch (port) {
	case 0xE6:
		return counter & 0xff;
	case 0xE7:
		return (counter >> 8) & 0xff;
	default:
		assert (false);
		return 255;	// avoid warning
	}
}

void MSXE6Timer::writeIO(byte port, byte value, Emutime &time)
{
	assert (port == 0xE6);
	reference = time;	// freq does not change
}

