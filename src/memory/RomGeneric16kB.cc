// $Id$

#include "RomGeneric16kB.hh"


RomGeneric16kB::RomGeneric16kB(Device* config, const EmuTime &time)
	: MSXDevice(config, time), Rom16kBBlocks(config, time)
{
	reset(time);
}

RomGeneric16kB::~RomGeneric16kB()
{
}

void RomGeneric16kB::reset(const EmuTime &time)
{
	setBank(0, unmapped);
	setRom (1, 0);
	setRom (2, 1);
	setBank(3, unmapped);
}

void RomGeneric16kB::writeMem(word address, byte value, const EmuTime &time)
{
	setRom(address >> 14, value);
}
