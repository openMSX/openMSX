// $Id$

/*
 * Submitted By: Albert Beevendorp (bifimsx)
 *
 * Some Sony MSX machines have built-in firmware
 * containing an address 'book', memo and scheduler and a
 * few of these can store these data on a data cartridge.
 *
 * In Basic it's possible to use it with the "CAT:" device.
 *
 * I'm not sure if there are more types, though the HBI-55
 * is a 4KB SRAM containing cartridge accessed by I/O
 * ports connected to a 8255 chip:
 *
 * B0 = LSB address
 * B1 = MSB address (D7-D6 = 01 write, 11 read)
 * B2 = Data port
 * B3 = Access control
 *
 * Sample basic program:
 *
 * 10  FOR I=0 TO 4095: OUT &HB3,128: OUT &HB0,I MOD 256:
 *     OUT &HB1,64 OR I\256: OUT &HB2,I MOD 256: NEXT I
 * 20  FOR I=0 TO 4095:OUT &HB3,128-9*(I<>0): OUT &HB0,I MOD 256:
 *     OUT &HB1,192 OR I\256: IF I MOD 256=INP(&HB2) THEN NEXT
 *     ELSE PRINT "Error comparing byte:";I: END
 * 30  PRINT "Done!"
 */

#include <cassert>
#include "MSXHBI55.hh"

namespace openmsx {

// MSXDevice

MSXHBI55::MSXHBI55(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  sram(getName() + " SRAM", 0x1000, config)
{
	i8255 = new I8255(*this, time);

	reset(time);
}

MSXHBI55::~MSXHBI55()
{
	delete i8255;
}

void MSXHBI55::reset(const EmuTime& time)
{
	i8255->reset(time);
}

byte MSXHBI55::readIO(byte port, const EmuTime& time)
{
	byte result;
	switch (port & 0x03) {
	case 0:
		result = i8255->readPortA(time);
		break;
	case 1:
		result = i8255->readPortB(time);
		break;
	case 2:
		result = i8255->readPortC(time);
		break;
	case 3:
		result = i8255->readControlPort(time);
		break;
	default: // unreachable, avoid warning
		assert(false);
		result = 0;
	}
	//PRT_DEBUG("HBI-55 read "<<hex<<(int)port<<" "<<(int)result<<dec);
	return result;
}

void MSXHBI55::writeIO(byte port, byte value, const EmuTime& time)
{
	//PRT_DEBUG("HBI-55 write "<<hex<<(int)port<<" "<<(int)value<<dec);
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

byte MSXHBI55::readA(const EmuTime& time)
{
	// TODO check this
	return 255;
}
void MSXHBI55::writeA(byte value, const EmuTime& time)
{
	address = (address & 0x0F00) | value; 
}

byte MSXHBI55::readB(const EmuTime& time)
{
	// TODO check this
	return 255;
}
void MSXHBI55::writeB(byte value, const EmuTime& time)
{
	address = (address & 0x00FF) | ((value & 0x0F) << 8);
	mode = value & 0xC0;
}

nibble MSXHBI55::readC1(const EmuTime& time)
{
	byte result;
	if (mode == 0xC0) {
		// read mode
		result = readSRAM(address) >> 4;
	} else {
		// TODO check
		result = 15;
	}
	return result;
}
nibble MSXHBI55::readC0(const EmuTime& time)
{
	byte result;
	if (mode == 0xC0) {
		// read mode
		result = readSRAM(address) & 0x0F;
	} else {
		// TODO check
		result = 15;
	}
	return result;
}
void MSXHBI55::writeC1(nibble value, const EmuTime& time)
{
	if (mode == 0x40) {
		// write mode
		byte tmp = readSRAM(address);
		sram[address] = (tmp & 0x0F) | (value << 4);
	} else {
		// TODO check
		// nothing 
	}
}
void MSXHBI55::writeC0(nibble value, const EmuTime& time)
{
	if (mode == 0x40) {
		// write mode
		byte tmp = readSRAM(address);
		sram[address] = (tmp & 0xF0) | value;
	} else {
		// TODO check
		// nothing 
	}
}

byte MSXHBI55::readSRAM(word address)
{
	if (address != 0) {
		return sram[address];
	} else {
		return 0x53;
	}
}

} // namespace openmsx
