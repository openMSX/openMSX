// $Id$

// ASCII 8kB based cartridges with SRAM
//   - ASCII8-8  /  KOEI-8  /  KOEI-32  /  WIZARDRY
//
// The address to change banks:
//  bank 1: 0x6000 - 0x67ff (0x6000 used)
//  bank 2: 0x6800 - 0x6fff (0x6800 used)
//  bank 3: 0x7000 - 0x77ff (0x7000 used)
//  bank 4: 0x7800 - 0x7fff (0x7800 used)
//
//  To select SRAM set bit 7 (for WIZARDRY) or the bit just above the
//  rom selection bits (bit 5/6/7 depending on ROM size). For KOEI-32
//  the lowest bits indicate which SRAM page is selected. SRAM is
//  readable at 0x8000-0xBFFF. For the KOEI-x types SRAM is also
//  readable at 0x4000-0x5FFF

#include "RomAscii8_8.hh"
#include "Rom.hh"
#include "SRAM.hh"

namespace openmsx {

RomAscii8_8::RomAscii8_8(
	MSXMotherBoard& motherBoard, const XMLElement& config,
        const EmuTime& time, std::auto_ptr<Rom> rom_, SubType subType)
	: Rom8kBBlocks(motherBoard, config, time, rom_)
	, sram(new SRAM(motherBoard, getName() + " SRAM",
	                (subType == KOEI_32) ? 0x8000 : 0x2000, config))
{
	sramEnableBit = (subType == WIZARDRY) ? 0x80
	                                      : rom->getSize() / 0x2000;
	sramPages = ((subType == KOEI_8) || (subType == KOEI_32))
	          ? 0x34 : 0x30;

	reset(time);
}

RomAscii8_8::~RomAscii8_8()
{
}

void RomAscii8_8::reset(const EmuTime& /*time*/)
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

void RomAscii8_8::writeMem(word address, byte value, const EmuTime& /*time*/)
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		// bank switching
		byte region = ((address >> 11) & 3) + 2;
		if (value & sramEnableBit) {
			sramEnabled |= (1 << region) & sramPages;
			sramBlock[region] = value & ((sram->getSize() / 0x2000) - 1);
			setBank(region, &(*sram)[sramBlock[region] * 0x2000]);
		} else {
			sramEnabled &= ~(1 << region);
			setRom(region, value);
		}
	} else {
		byte bank = address >> 13;
		if ((1 << bank) & sramEnabled) {
			// write to SRAM
			word addr = (sramBlock[bank] * 0x2000) + (address & 0x1FFF);
			sram->write(addr, value);
		}
	}
}

byte* RomAscii8_8::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x8000)) {
		// bank switching
		return NULL;
	} else if ((1 << (address >> 13)) & sramEnabled) {
		// write to SRAM
		return NULL;
	} else {
		return unmappedWrite;
	}
}

} // namespace openmsx
