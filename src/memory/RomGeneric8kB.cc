// $Id$

#include "RomGeneric8kB.hh"


RomGeneric8kB::RomGeneric8kB(Device* config, const EmuTime &time)
	: MSXDevice(config, time), Rom8kBBlocks(config, time)
{
	reset(time);
}

RomGeneric8kB::~RomGeneric8kB()
{
}

void RomGeneric8kB::reset(const EmuTime &time)
{
	setBank(0, unmapped);
	setBank(1, unmapped);
	for (int i = 2; i < 6; i++) {
		setRom(i, i - 2);
	}
	setBank(6, unmapped);
	setBank(7, unmapped);
}

void RomGeneric8kB::writeMem(word address, byte value, const EmuTime &time)
{
	setRom(address >> 13, value);
}
