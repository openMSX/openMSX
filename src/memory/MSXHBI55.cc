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
 *     10 FOR I=0 TO 4095: OUT &HB3,128: OUT &HB0,I MOD 256:
 *        OUT &HB1,64 OR I\256: OUT &HB2,I MOD 256: NEXT I
 *     20 FOR I=0 TO 4095:OUT &HB3,128-9*(I<>0): OUT &HB0,I MOD 256:
 *        OUT &HB1,192 OR I\256: IF I MOD 256=INP(&HB2) THEN NEXT
 *        ELSE PRINT "Error comparing byte:";I: END
 *     30 PRINT "Done!"
 * Update:
 *   This test-program is not correct, see the link below for improved
 *   version(s).
 *
 * -----
 *
 * Improved code mostly copied from blueMSX, many thanks to Daniel Vik.
 *   http://cvs.sourceforge.net/viewcvs.py/bluemsx/blueMSX/Src/Memory/romMapperSonyHBI55.c
 * Update:
 *   Later the implementation was simplified quite a bit by replacing internal
 *   state with looking at the current PPI output values. This is closer to how
 *   the real hardware works.
 *
 * When only one of the nibbles is written (actually when one is set as
 * input the other as output), the other nibble gets the value of the
 * last time that nibble was written. Thanks to Laurens Holst for
 * investigating this. See this bug report for more details:
 *   https://sourceforge.net/p/openmsx/bugs/536/
 * Update:
 *   This behavior is actually undefined: it writes a 'floating value'.
 *   This value happens to (mostly) be the last written value, at least if
 *   there's not too much time between the writes. Nevertheless we currently
 *   emulate it like this.
 *
 * For more details see the bug report(s) at:
 *   https://github.com/openMSX/openMSX/issues/927
 * Especially see grauw's (updated) test programs (2022/02/20).
 *
 * See also the (updated) documentation on:
 *   http://map.grauw.nl/resources/hbi55.php
 * And the service manual (contains electrical schema):
 *   http://map.grauw.nl/resources/memory/sony_hbi-55_sm.pdf
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
	lastC = 255; // hack
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
	return 255; // TODO check this
}
byte MSXHBI55::readB(EmuTime::param time)
{
	return peekB(time);
}
byte MSXHBI55::peekB(EmuTime::param /*time*/) const
{
	return 255; // TODO check this
}
nibble MSXHBI55::readC0(EmuTime::param time)
{
	return peekC0(time);
}
nibble MSXHBI55::peekC0(EmuTime::param /*time*/) const
{
	return readStuff() & 0x0F;
}
nibble MSXHBI55::readC1(EmuTime::param time)
{
	return peekC1(time);
}
nibble MSXHBI55::peekC1(EmuTime::param /*time*/) const
{
	return readStuff() >> 4;
}

void MSXHBI55::writeA(byte /*value*/, EmuTime::param /*time*/)
{
	writeStuff();
}
void MSXHBI55::writeB(byte /*value*/, EmuTime::param /*time*/)
{
	writeStuff();
}
void MSXHBI55::writeC0(nibble value, EmuTime::param /*time*/)
{
	lastC = (lastC & 0xf0) | value; // hack
	writeStuff();
}
void MSXHBI55::writeC1(nibble value, EmuTime::param /*time*/)
{
	lastC = (lastC & 0x0f) | (value << 4); // hack
	writeStuff();
}

void MSXHBI55::writeStuff()
{
	byte B = i8255.getPortB();
	if ((B & 0x70) != 0x40) {
		// /CE of RAM chip(s) not active
		return;
	}

	if (B & 0x80) {
		// read, do nothing
	} else {
		// write
		byte A = i8255.getPortA();
		unsigned addr = ((B & 0x0f) << 8) | A;
		// In the normal case port C should be programmed as an output,
		// and then we can get the value like this:
		//     byte C = i8255.getPortC();
		// When port C is (partially) set as input, we technically use a
		// 'floating value'. Hack: experiments have shown that we more
		// closely _approximate_ this (invalid) case by using the last
		// written (output-)value.
		byte C = lastC;
		sram.write(addr, C);
	}
}

byte MSXHBI55::readStuff() const
{
	byte B = i8255.getPortB();
	if ((B & 0x70) != 0x40) {
		// /CE of RAM chip(s) not active
		return lastC; // actually floating value
	}

	if (B & 0x80) {
		// read
		byte A = i8255.getPortA();
		unsigned addr = ((B & 0x0f) << 8) | A;
		return sram[addr];
	} else {
		// write
		// Normally this shouldn't occur: when writing, port C should be
		// programmed as an output, and then this method won't get called.
		return lastC; // actually floating value
	}
}

// version 1: initial version
// version 2: removed 'readAddress', 'writeAddress', 'addressLatch', 'writeLatch', 'mode'
//                    these are replaced by i8255.getPort{A,B,C}().
//            introduced 'lastC'
//                    This is a hack: it's used instead of i8255.getPortC().
//                    This more or less maps to the old 'writeLatch' variable.
template<typename Archive>
void MSXHBI55::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("i8255", i8255,
	             "SRAM",  sram);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("lastC", lastC);
	} else {
		assert(Archive::IS_LOADER);
		byte writeLatch = 0;
		ar.serialize("writeLatch", writeLatch);
		lastC = writeLatch;
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXHBI55);
REGISTER_MSXDEVICE(MSXHBI55, "MSXHBI55");

} // namespace openmsx
