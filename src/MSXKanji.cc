// $Id$

#include <cassert>
#include "MSXKanji.hh"


MSXKanji::MSXKanji(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time),
	  rom(config, time)
{
	int size = rom.getSize();
	if ((size != 0x20000) && (size != 0x40000)) {
		PRT_ERROR("MSXKanji: wrong kanji rom");
	}

	reset(time);
}

MSXKanji::~MSXKanji()
{
}

void MSXKanji::reset(const EmuTime &time)
{
	adr1 = 0;	// TODO check this
	adr2 = 0x20000;	// TODO check this
}

void MSXKanji::writeIO(byte port, byte value, const EmuTime &time)
{
	//PRT_DEBUG("MSXKanji: write " << (int)port << " " << (int)value);
	switch (port & 0x03) {
	case 0:
		adr1 = (adr1 & 0x1f800) | ((value & 0x3f) << 5);
		break;
	case 1:
		adr1 = (adr1 & 0x007e0) | ((value & 0x3f) << 11);
		break;
	case 2:
		adr2 = (adr2 & 0x3f800) | ((value & 0x3f) << 5);
		break;
	case 3:
		adr2 = (adr2 & 0x207e0) | ((value & 0x3f) << 11);
		break;
	default:
		assert(false);
	}
}

byte MSXKanji::readIO(byte port, const EmuTime &time)
{
	byte result;
	switch (port & 0x03) {
	case 1:
		result = rom.read(adr1);
		adr1 = (adr1 & ~0x1f) | ((adr1 + 1) & 0x1f);
		break;
	case 3:
		if (rom.getSize() == 0x40000) { // temp workaround
			result = rom.read(adr2);
			adr2 = (adr2 & ~0x1f) | ((adr2 + 1) & 0x1f);
		} else {
			result = 0xFF;
		}
		break;
	default:
		// This port should not have been registered.
		assert(false);
		result = 0xFF;
	}
	//PRT_DEBUG("MSXKanji: read " << (int)port << " " << (int)result);
	return result;
}

/*
This really works!

10 DIM A(32)
20 FOR I=0 TO 4095
30 OUT &HD8, I MOD 64: OUT &HD9, I\64
40 FOR J=0 TO 31: A(J)=INP(&HD9): NEXT
50 FOR J=0 TO 7
60 PRINT RIGHT$("0000000"+BIN$(A(J)), 8);
70 PRINT RIGHT$("0000000"+BIN$(A(J+8)), 8)
80 NEXT
90 FOR J=16 TO 23
100 PRINT RIGHT$("0000000"+BIN$(A(J)), 8);
110 PRINT RIGHT$("0000000"+BIN$(A(J+8)), 8)
120 NEXT
130 PRINT
140 NEXT
*/

