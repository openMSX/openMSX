// $Id$

// ASCII 8kB cartridges with 8kB SRAM
// 
// this type is used in Xanadu and Royal Blood
//
// The address to change banks:
//  bank 1: 0x6000 - 0x67ff (0x6000 used)
//  bank 2: 0x6800 - 0x6fff (0x6800 used)
//  bank 3: 0x7000 - 0x77ff (0x7000 used)
//  bank 4: 0x7800 - 0x7fff (0x7800 used)
//
//  To select SRAM set bit 5/6/7 (depends on size) of the bank.
//  The SRAM can only be written to if selected in bank 3 or 4.

#include "RomAscii8_8.hh"
#include "MSXConfig.hh"


RomAscii8_8::RomAscii8_8(Device* config, const EmuTime &time, Rom *rom)
	: MSXDevice(config, time), Rom8kBBlocks(config, time, rom),
	  sram(0x2000, config)
{
	reset(time);
}

RomAscii8_8::~RomAscii8_8()
{
}

void RomAscii8_8::reset(const EmuTime &time)
{
	setBank(0, unmappedRead);
	setBank(1, unmappedRead);
	for (int i = 2; i < 6; i++) {
		setRom(i, 0);
	}
	setBank(6, unmappedRead);
	setBank(7, unmappedRead);

	sramEnabled = 0;
}

void RomAscii8_8::writeMem(word address, byte value, const EmuTime &time)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		// bank switching
		byte region = ((address >> 11) & 3) + 2;
		byte sramEnableBit = rom->getSize() / 8192;
		if (value & sramEnableBit) {
			setBank(region, sram.getBlock());
			sramEnabled |= (1 << region);
		} else {
			setRom(region, value);
			sramEnabled &= ~(1 << region);
		}
	} else if ((1 << (address >> 13)) & sramEnabled & 0x30) {
		// write to SRAM
		sram.write(address & 0x1FFF, value);
	}
}

byte* RomAscii8_8::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		// bank switching
		return NULL;
	} else if ((1 << (address >> 13)) & sramEnabled & 0x30) {
		// write to SRAM
		return sram.getBlock(address & 0x1FFF);
	} else {
		return unmappedWrite;
	}
}
