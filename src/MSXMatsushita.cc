// $Id$

#include "MSXMatsushita.hh"


const byte ID = 0x08;

MSXMatsushita::MSXMatsushita(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXSwitchedDevice(ID),
	  sram(0x800, config)
{
	// TODO find out what ports 0x41 0x45 0x46 are used for
	//      (and if they belong to this device)

	reset(time);
}

MSXMatsushita::~MSXMatsushita()
{
}

void MSXMatsushita::reset(const EmuTime &time)
{
	address = 0;	// TODO check this
}

byte MSXMatsushita::readIO(byte port, const EmuTime &time)
{
	// TODO can you read from 0x47 0x48
	byte result;
	switch (port) {
		case 0x40:
			result = ~ID;
			break;
		case 0x43:
			result = ((pattern & 0x80) ? color2 : color1) << 4 |
				 ((pattern & 0x40) ? color2 : color1);
			pattern = (pattern << 2) | (pattern >> 6);
			break;
		case 0x49:
			result = sram.read(address);
			address = (address + 1) & 0x7FF;
			break;
		default:
			result = 255;
	}
	return result;
}

void MSXMatsushita::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
		case 0x43:
			color2 = (value & 0xF0) >> 4;
			color1 =  value & 0x0F;
			break;
		case 0x44:
			pattern = value;
			break;
		case 0x47:
			// set address (low)
			address = (address & 0xFF00) | value;
			break;
		case 0x48:
			// set address (high)
			address = (address & 0x00FF) | ((value & 0x07) << 8);
			break;
		case 0x49:
			// write sram
			sram.write(address, value);
			address = (address + 1) & 0x7FF;
			break;
	}
}
