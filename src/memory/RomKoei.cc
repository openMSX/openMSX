// $Id$

// Koei mapper type with 8 or 32kB SRAM (very similar to ASCII8-8)
//
// based on code from BlueMSX v1.4.0
// 
// The address to change banks:
//  bank 1: 0x6000 - 0x67ff (0x6000 used)
//  bank 2: 0x6800 - 0x6fff (0x6800 used)
//  bank 3: 0x7000 - 0x77ff (0x7000 used)
//  bank 4: 0x7800 - 0x7fff (0x7800 used)
//
//  To select SRAM set any of the higher bits (for ASCII8-8 only one bit can be
//  used). The SRAM can also be writen to in 0x4000-0x5FFF range (ASCII8-8 only
//  0x8000-0xBFFF).

#include "RomKoei.hh"

namespace openmsx {

RomKoei::RomKoei(Config* config, const EmuTime& time, Rom* rom, SramSize sramSize)
	: MSXDevice(config, time), Rom8kBBlocks(config, time, rom),
	  sram(0x2000 * (int)sramSize, config)
{
	reset(time);
}

RomKoei::~RomKoei()
{
}

void RomKoei::reset(const EmuTime& time)
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

void RomKoei::writeMem(word address, byte value, const EmuTime& time)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		// bank switching
		byte region = ((address >> 11) & 3) + 2;
		byte sramEnableBit = rom->getSize() / 0x2000;
		if (value >= sramEnableBit) {
			sramBlock[region] = value & ((sram.getSize() / 0x2000) - 1);
			setBank(region, sram.getBlock(sramBlock[region] * 0x2000));
			sramEnabled |= (1 << region);
		} else {
			setRom(region, value);
			sramEnabled &= ~(1 << region);
		}
	} else {
		byte bank = address >> 13;
		if ((1 << bank) & sramEnabled & 0x34) {
			// write to SRAM
			word addr = (sramBlock[bank] * 0x2000) + (address & 0x1FFF);
			sram.write(addr, value);
		}
	}
}

byte* RomKoei::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		// bank switching
		return NULL;
	} else if ((1 << (address >> 13)) & sramEnabled & 0x34) {
		// write to SRAM
		byte bank = address >> 13;
		word addr = (sramBlock[bank] * 0x2000) + (address & 0x1FFF);
		return sram.getBlock(addr);
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx
