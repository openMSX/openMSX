// $Id$

#include <cassert>
#include "MSXHBI55.hh"
#include "MSXConfig.hh"


// MSXDevice

MSXHBI55::MSXHBI55(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  sram(0x1000, config)
{
	i8255 = new I8255(*this, time);

	reset(time);
}

MSXHBI55::~MSXHBI55()
{
	delete i8255;
}

void MSXHBI55::reset(const EmuTime &time)
{
	i8255->reset(time);
}

byte MSXHBI55::readIO(byte port, const EmuTime &time)
{
	switch (port & 0x03) {
	case 0:
		return i8255->readPortA(time);
	case 1:
		return i8255->readPortB(time);
	case 2:
		return i8255->readPortC(time);
	case 3:
		return i8255->readControlPort(time);
	default: // unreachable, avoid warning
		assert(false);
		return 0;
	}
}

void MSXHBI55::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port & 0x03) {
	case 0:
		i8255->writePortA(value, time);
		break;
	case 1:
		i8255->writePortB(value, time);
		break;
	case 2:
		i8255->writePortC(value, time);
		break;
	case 3:
		i8255->writeControlPort(value, time);
		break;
	default:
		assert(false);
	}
}


// I8255Interface

byte MSXHBI55::readA(const EmuTime &time)
{
	// TODO check this
	return 255;
}
void MSXHBI55::writeA(byte value, const EmuTime &time)
{
	address = (address & 0x0F00) | value; 
}

byte MSXHBI55::readB(const EmuTime &time)
{
	// TODO check this
	return 255;
}
void MSXHBI55::writeB(byte value, const EmuTime &time)
{
	address = (address & 0x00FF) | ((value & 0x0F) << 8);
	mode = value & 0xC0;
}

nibble MSXHBI55::readC1(const EmuTime &time)
{
	if (mode == 0xC0) {
		// read mode
		return sram.read(address) >> 4;
	} else {
		// TODO check
		return 15;
	}
}
nibble MSXHBI55::readC0(const EmuTime &time)
{
	if (mode == 0xC0) {
		// read mode
		return sram.read(address) & 0x0F;
	} else {
		// TODO check
		return 15;
	}
}
void MSXHBI55::writeC1(nibble value, const EmuTime &time)
{
	if (mode == 0x40) {
		// write mode
		byte tmp = sram.read(address);
		sram.write(address, (tmp & 0x0F) | (value << 4));
	} else {
		// TODO check
		// nothing 
	}
}
void MSXHBI55::writeC0(nibble value, const EmuTime &time)
{
	if (mode == 0x40) {
		// write mode
		byte tmp = sram.read(address);
		sram.write(address, (tmp & 0xF0) | value);
	} else {
		// TODO check
		// nothing 
	}
}
