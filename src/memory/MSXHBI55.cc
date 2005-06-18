// $Id$

/* Submitted By: Albert Beevendorp (bifimsx)
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
 *
 * -----
 *
 * Improved code mostly copied from blueMSX, many thanks to Daniel Vik.
 * http://cvs.sourceforge.net/viewcvs.py/bluemsx/blueMSX/Src/Memory/romMapperSonyHBI55.c
 */

#include "MSXHBI55.hh"
#include "SRAM.hh"
#include <cassert>

namespace openmsx {

// MSXDevice

MSXHBI55::MSXHBI55(MSXMotherBoard& motherBoard, const XMLElement& config,
                   const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, i8255(new I8255(*this, time))
	, sram(new SRAM(motherBoard, getName() + " SRAM", 0x1000, config))
{

	reset(time);
}

MSXHBI55::~MSXHBI55()
{
}

void MSXHBI55::reset(const EmuTime& time)
{
	readAddress = 0;
	writeAddress = 0;
	addressLatch = 0;
	writeLatch = 0;
	mode = 0;
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

byte MSXHBI55::peekIO(byte port, const EmuTime& time) const
{
	byte result;
	switch (port & 0x03) {
	case 0:
		result = i8255->peekPortA(time);
		break;
	case 1:
		result = i8255->peekPortB(time);
		break;
	case 2:
		result = i8255->peekPortC(time);
		break;
	case 3:
		result = i8255->readControlPort(time);
		break;
	default: // unreachable, avoid warning
		assert(false);
		result = 0;
	}
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
	return peekA(time);
}
byte MSXHBI55::peekA(const EmuTime& /*time*/) const
{
	// TODO check this
	return 255;
}
byte MSXHBI55::readB(const EmuTime& time)
{
	return peekB(time);
}
byte MSXHBI55::peekB(const EmuTime& /*time*/) const
{
	// TODO check this
	return 255;
}

void MSXHBI55::writeA(byte value, const EmuTime& /*time*/)
{
	addressLatch = value;
}
void MSXHBI55::writeB(byte value, const EmuTime& /*time*/)
{
	word address = addressLatch | ((value & 0x0F) << 8);
	mode = value >> 6;
	switch (mode) {
		case 0:
			readAddress = 0;
			writeAddress = 0;
			break;
		case 1:
			writeAddress = address;
			break;
		case 2:
			sram->write(writeAddress, writeLatch);
			break;
		case 3:
			readAddress = address;
			break;
	}
}

nibble MSXHBI55::readC0(const EmuTime& time)
{
	return peekC0(time);
}
nibble MSXHBI55::peekC0(const EmuTime& /*time*/) const
{
	return readSRAM(readAddress) & 0x0F;
}
nibble MSXHBI55::readC1(const EmuTime& time)
{
	return peekC1(time);
}
nibble MSXHBI55::peekC1(const EmuTime& /*time*/) const
{
	return readSRAM(readAddress) >> 4;
}

void MSXHBI55::writeC0(nibble value, const EmuTime& /*time*/)
{
	writeLatch = (writeLatch & 0xF0) | value;
}
void MSXHBI55::writeC1(nibble value, const EmuTime& /*time*/)
{
	writeLatch = (writeLatch & 0x0F) | (value << 4);
	if (mode == 1) {
		sram->write(writeAddress, writeLatch);
	}
}

byte MSXHBI55::readSRAM(word address) const
{
	return address ? (*sram)[address] : 0x53;
}

} // namespace openmsx
