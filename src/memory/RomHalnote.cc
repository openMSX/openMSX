// $Id$

#include "RomHalnote.hh"


RomHalnote::RomHalnote(Device* config, const EmuTime &time)
	: MSXDevice(config, time), Rom8kBBlocks(config, time)
{
	reset(time);
}

RomHalnote::~RomHalnote()
{
}

void RomHalnote::reset(const EmuTime &time)
{
	setBank(0, unmapped);
	setBank(1, unmapped);
	for (int i = 2; i < 6; i++) {
		setRom(i, 0);
	}
	setBank(6, unmapped);
	setBank(7, unmapped);
}

void RomHalnote::writeMem(word address, byte value, const EmuTime &time)
{
	if ((0x4000 <= address) && (address < 0xC000)) {
		if ((address & 0x0FFF) == 0x0FFF) {
			setRom(address >> 13, value);
		}
	}
}
