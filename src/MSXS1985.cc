// $Id$

#include "MSXS1985.hh"


namespace openmsx {

const byte ID = 0xFE;

MSXS1985::MSXS1985(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXSwitchedDevice(ID)
{
	// TODO load ram
}

MSXS1985::~MSXS1985()
{
	// TODO save ram
}


void MSXS1985::reset(const EmuTime &time)
{
}

byte MSXS1985::readIO(byte port, const EmuTime &time)
{
	byte result;
	switch (port & 0x0F) {
	case 0:
		result = (byte)~ID;
		break;
	case 2:
		result = ram[address];
		break;
	case 7:
		result = (pattern & 0x80) ? color2 : color1;
		pattern = (pattern << 1) | (pattern >> 7);
		break;
	default:
		result = 0xFF;
	}
	//PRT_DEBUG("S1985: read " << (int) port << " " << (int)result);
	return result;
}

void MSXS1985::writeIO(byte port, byte value, const EmuTime &time)
{
	//PRT_DEBUG("S1985: write " << (int) port << " " << (int)value);
	switch (port & 0x0F) {
	case 1:
		address = value & 0x0F;
		break;
	case 2:
		ram[address] = value;
		break;
	case 6:
		color2 = color1;
		color1 = value;
		break;
	case 7:
		pattern = value;
		break;
	}
}

} // namespace openmsx
