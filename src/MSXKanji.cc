// $Id$

#include "config.h"

#include <fstream>
#include <iostream>
#include <cassert>
#include "MSXKanji.hh"
#include "MSXMotherBoard.hh"


MSXKanji::MSXKanji()
{
}

MSXKanji::~MSXKanji()
{
	delete [] buffer;
}

void MSXKanji::init()
{
	MSXDevice::init();
	
	buffer = new byte[ROM_SIZE];
	if (buffer==NULL)
		PRT_ERROR("Couldn't allocate Kanji buffer");
#ifdef HAVE_FSTREAM_TEMPL
	std::ifstream<byte> file("KANJI.ROM");
#else
	std::ifstream file("KANJI.ROM");
#endif
	file.read(buffer, ROM_SIZE);
	if (file.fail())
		PRT_ERROR("Couldn't read from KANJI.ROM");
	file.close();
	
	MSXMotherBoard::instance()->register_IO_In (0xD9, this);
	MSXMotherBoard::instance()->register_IO_Out(0xD8, this);
	MSXMotherBoard::instance()->register_IO_Out(0xD9, this);
}

void MSXKanji::reset()
{
	adr = count = 0;	// TODO check this
}

byte MSXKanji::readIO(byte port, Emutime &time)
{
	assert(port==0xD9);
	byte tmp = buffer[adr+count];
	count = (count+1)&0x1f;		//TODO check counter behaviour
	return tmp;
}

void MSXKanji::writeIO(byte port, byte value, Emutime &time)
{
	//TODO check this
	switch (port) {
	case 0xd8:
		adr = (adr&0x1f800)|((value&0x3f)<<5);
		count = 0;
		break;
	case 0xd9:
		adr = (adr&0x007e0)|((value&0x3f)<<11);
		count = 0;
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


