// $Id$

#include "RomCrossBlaim.hh"


RomCrossBlaim::RomCrossBlaim(Device* config, const EmuTime &time)
	: MSXDevice(config, time), Rom16kBBlocks(config, time)
{
	reset(time);
}

RomCrossBlaim::~RomCrossBlaim()
{
}

void RomCrossBlaim::reset(const EmuTime &time)
{
	setBank(0, unmapped);
	setRom (1, 0);
	setRom (2, 0);
	setBank(3, unmapped);
}

void RomCrossBlaim::writeMem(word address, byte value, const EmuTime &time)
{
	if (address == 0x4045) {
		setRom(2, value);
	}
}
