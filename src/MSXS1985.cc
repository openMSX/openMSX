// $Id$

#include "MSXS1985.hh"
#include "MSXCPUInterface.hh"


const byte ID = 0xFE;

MSXS1985::MSXS1985(MSXConfig::Device *config, const EmuTime &time)
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
	switch (port) {
		case 0x40:
			result = ~ID;
			break;
		case 0x42:
			result = ram[address];
			break;
		case 0x47:
			result = (pattern & 0x80) ? color2 : color1;
			pattern = (pattern << 1) | (pattern >> 7);
			break;
		default:
			result = 255;
	}
	//PRT_DEBUG("S1985: read " << (int) port << " " << (int)result);
	return result;
}

void MSXS1985::writeIO(byte port, byte value, const EmuTime &time)
{
	//PRT_DEBUG("S1985: write " << (int) port << " " << (int)value);
	switch (port) {
		case 0x41:
			address = value & 0x0F;
			break;
		case 0x42:
			ram[address] = value;
			break;
		case 0x46:
			color2 = color1;
			color1 = value;
			break;
		case 0x47:
			pattern = value;
			break;
	}
}
