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
#include "GlobalSettings.hh"
#include "serialize.hh"

namespace openmsx {

// MSXDevice

MSXHBI55::MSXHBI55(const DeviceConfig& config)
	: MSXDevice(config)
	, i8255(*this, getCurrentTime(), config.getGlobalSettings().getInvalidPpiModeSetting())
	, sram(getName() + " SRAM", 0x1000, config)
{
	reset(getCurrentTime());
}

void MSXHBI55::reset(EmuTime::param time)
{
	readAddress = 0;
	writeAddress = 0;
	addressLatch = 0;
	writeLatch = 0;
	mode = 0;
	i8255.reset(time);
}

byte MSXHBI55::readIO(word port, EmuTime::param time)
{
	return i8255.read(port & 0x03, time);
}

byte MSXHBI55::peekIO(word port, EmuTime::param time) const
{
	return i8255.peek(port & 0x03, time);
}

void MSXHBI55::writeIO(word port, byte value, EmuTime::param time)
{
	i8255.write(port & 0x03, value, time);
}


// I8255Interface

byte MSXHBI55::readA(EmuTime::param time)
{
	return peekA(time);
}
byte MSXHBI55::peekA(EmuTime::param /*time*/) const
{
	// TODO check this
	return 255;
}
byte MSXHBI55::readB(EmuTime::param time)
{
	return peekB(time);
}
byte MSXHBI55::peekB(EmuTime::param /*time*/) const
{
	// TODO check this
	return 255;
}

void MSXHBI55::writeA(byte value, EmuTime::param /*time*/)
{
	addressLatch = value;
}
void MSXHBI55::writeB(byte value, EmuTime::param /*time*/)
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
		sram.write(writeAddress, writeLatch);
		break;
	case 3:
		readAddress = address;
		break;
	}
}

nibble MSXHBI55::readC0(EmuTime::param time)
{
	return peekC0(time);
}
nibble MSXHBI55::peekC0(EmuTime::param /*time*/) const
{
	return readSRAM(readAddress) & 0x0F;
}
nibble MSXHBI55::readC1(EmuTime::param time)
{
	return peekC1(time);
}
nibble MSXHBI55::peekC1(EmuTime::param /*time*/) const
{
	return readSRAM(readAddress) >> 4;
}

// When only one of the nibbles is written (actually when one is set as
// input the other as output), the other nibble gets the value of the
// last time that nibble was written. Thanks to Laurens Holst for
// investigating this. See this bug report for more details:
//   https://sourceforge.net/p/openmsx/bugs/536/
void MSXHBI55::writeC0(nibble value, EmuTime::param /*time*/)
{
	writeLatch = (writeLatch & 0xF0) | value;
	if (mode == 1) {
		sram.write(writeAddress, writeLatch);
	}
}
void MSXHBI55::writeC1(nibble value, EmuTime::param /*time*/)
{
	writeLatch = (writeLatch & 0x0F) | (value << 4);
	if (mode == 1) {
		sram.write(writeAddress, writeLatch);
	}
}

byte MSXHBI55::readSRAM(word address) const
{
	return address ? sram[address] : 0x53;
}

template<typename Archive>
void MSXHBI55::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("i8255",        i8255,
	             "SRAM",         sram,
	             "readAddress",  readAddress,
	             "writeAddress", writeAddress,
	             "addressLatch", addressLatch,
	             "writeLatch",   writeLatch,
	             "mode",         mode);
}
INSTANTIATE_SERIALIZE_METHODS(MSXHBI55);
REGISTER_MSXDEVICE(MSXHBI55, "MSXHBI55");

} // namespace openmsx
