// $Id$

#include "MC6850.hh"


MC6850::MC6850(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	reset(time);
}

MC6850::~MC6850()
{
}

void MC6850::reset(const EmuTime &time)
{
}

byte MC6850::readIO(byte port, const EmuTime &time)
{
	byte result;
	switch (port & 0x1) {
	case 0: // read status
		result = 2;
		break;
	case 1: // read data
		result = 0;
		break;
	default: // unreachable, avoid warning
		assert(false);
		result = 0;
	}
	PRT_DEBUG("Audio: read "<<std::hex<<(int)port<<" "<<(int)result<<std::dec);
	return result;
}

void MC6850::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port & 0x01) {
	case 0: // control
		break;
	case 1: // write data
		break;
	}
}
