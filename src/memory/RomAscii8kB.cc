// $Id$

// ASCII 8kB cartridges
// 
// this type is used in many japanese-only cartridges.
// example of cartridges: Valis(Fantasm Soldier), Dragon Slayer, Outrun, 
//                        Ashguine 2, ...
// The address to change banks:
//  bank 1: 0x6000 - 0x67ff (0x6000 used) 
//  bank 2: 0x6800 - 0x6fff (0x6800 used)
//  bank 3: 0x7000 - 0x77ff (0x7000 used)
//  bank 4: 0x7800 - 0x7fff (0x7800 used)

#include "RomAscii8kB.hh"


RomAscii8kB::RomAscii8kB(Device* config, const EmuTime &time)
	: MSXDevice(config, time), Rom8kBBlocks(config, time)
{
	reset(time);
}

RomAscii8kB::~RomAscii8kB()
{
}

void RomAscii8kB::reset(const EmuTime &time)
{
	setBank(0, unmapped);
	setBank(1, unmapped);
	for (int i = 2; i < 6; i++) {
		setRom(i, 0);
	}
	setBank(6, unmapped);
	setBank(7, unmapped);
}

void RomAscii8kB::writeMem(word address, byte value, const EmuTime &time)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		byte region = ((address >> 11) & 3) + 2;
		setRom(region, value);
	}
}
