
#include <fstream>
#include <iostream>
#include <assert.h>
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
	ifstream file("KANJI.ROM");
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
