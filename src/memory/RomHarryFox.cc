// $Id$

#include "RomHarryFox.hh"


RomHarryFox::RomHarryFox(Device* config, const EmuTime &time)
	: MSXDevice(config, time), Rom16kBBlocks(config, time)
{
	reset(time);
}

RomHarryFox::~RomHarryFox()
{
}

void RomHarryFox::reset(const EmuTime &time)
{
	setBank(0, unmapped);
	setRom (1, 0);
	setRom (2, 1);
	setBank(3, unmapped);
}

void RomHarryFox::writeMem(word address, byte value, const EmuTime &time)
{
	if (address == 0x6000) {
		setRom(1, 2 * (value & 1));
	}
	if (address == 0x7000) {
		setRom(2, 2 * (value & 1) + 1);
	}
}
