// $Id$

#include <cassert>
#include "MSXKanji.hh"
#include "MSXMotherBoard.hh"


MSXKanji::MSXKanji(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time), MSXRomDevice(config, time)
{
	try {
		loadFile(&memoryBank, ROM_SIZE);
	} catch (MSXException &e) {
		// TODO why isn't exception thrown beyond constructor
		PRT_ERROR(e.desc);
	}
	
	MSXMotherBoard::instance()->register_IO_In (0xD9, this);
	MSXMotherBoard::instance()->register_IO_Out(0xD8, this);
	MSXMotherBoard::instance()->register_IO_Out(0xD9, this);
	
	MSXMotherBoard::instance()->register_IO_In (0xDB, this);
	MSXMotherBoard::instance()->register_IO_Out(0xDA, this);
	MSXMotherBoard::instance()->register_IO_Out(0xDB, this);

	reset(time);
}

MSXKanji::~MSXKanji()
{
}

void MSXKanji::reset(const EmuTime &time)
{
	adr1 = count1 = 0;	// TODO check this
	adr2 = count2 = 0;	// TODO check this
}

byte MSXKanji::readIO(byte port, const EmuTime &time)
{
	byte tmp;
	switch (port) {
	case 0xd9:
		tmp = memoryBank[adr1+count1];
		count1 = (count1+1)&0x1f;		//TODO check counter behaviour
		return tmp;
	case 0xdb:
		tmp = memoryBank[adr2+count2+0x20000];
		count2 = (count2+1)&0x1f;		//TODO check counter behaviour
		return tmp;
	default:
		assert(false);
		return 0xff;	// prevent warning
	}
}

void MSXKanji::writeIO(byte port, byte value, const EmuTime &time)
{
	//TODO check this
	switch (port) {
	case 0xd8:
		adr1 = (adr1&0x1f800)|((value&0x3f)<<5);
		count1 = 0;
		break;
	case 0xd9:
		adr1 = (adr1&0x007e0)|((value&0x3f)<<11);
		count1 = 0;
		break;
	case 0xda:
		adr2 = (adr2&0x1f800)|((value&0x3f)<<5);
		count2 = 0;
		break;
	case 0xdb:
		adr2 = (adr2&0x007e0)|((value&0x3f)<<11);
		count2 = 0;
		break;
	default:
		assert(false);
	}
}

// This really works!
//
// 10 DIM A(32)
// 20 FOR I=0 TO 4095
// 30 OUT &HD8, I MOD 64: OUT &HD9, I\64
// 40 FOR J=0 TO 31: A(J)=INP(&HD9): NEXT
// 50 FOR J=0 TO 7
// 60 PRINT RIGHT$("0000000"+BIN$(A(J)), 8);
// 70 PRINT RIGHT$("0000000"+BIN$(A(J+8)), 8)
// 80 NEXT
// 90 FOR J=16 TO 23
// 100 PRINT RIGHT$("0000000"+BIN$(A(J)), 8);
// 110 PRINT RIGHT$("0000000"+BIN$(A(J+8)), 8)
// 120 NEXT
// 130 PRINT
// 140 NEXT


