// $Id$

#include "RomPanasonic.hh"
#include "CPU.hh"


RomPanasonic::RomPanasonic(Device* config, const EmuTime &time)
	: MSXDevice(config, time), Rom8kBBlocks(config, time)
{
	reset(time);
}

RomPanasonic::~RomPanasonic()
{
}

void RomPanasonic::reset(const EmuTime &time)
{
	control = 0;
	for (int region = 0; region < 8; region++) {
		setRom(region, 0);
		bankSelect[region] = 0;
	}
}

byte RomPanasonic::readMem(word address, const EmuTime &time)
{
	if ((control & 0x04) && (0x7FF0 <= address) && (address < 0x7FF7)) {
		// read mapper state (lower 8 bit)
		return bankSelect[address & 7] & 0xFF;
	}
	if ((control & 0x10) && (address == 0x7FF8)) {
		// read mapper state (9th bit)
		byte res = 0;
		for (int i = 7; i >= 0; i--) {
			if (bankSelect[i] & 0x100) {
				res++;
			}
			res <<= 1;
		}
		return res;
	}
	if ((control & 0x08) && (address == 0x7FF9)) {
		// read control byte
		return control;
	}
	return Rom8kBBlocks::readMem(address, time);
}

const byte* RomPanasonic::getReadCacheLine(word address) const
{
	if ((0x7FF0 & CPU::CACHE_LINE_HIGH) == address) {
		// TODO check mirrored
		return NULL;
	} else {
		return Rom8kBBlocks::getReadCacheLine(address);
	}
}

void RomPanasonic::writeMem(word address, byte value, const EmuTime &time)
{
	if ((0x6000 <= address) && (address < 0x7FF0)) {
		// set mapper state (lower 8 bits)
		int region = (address & 0x1C00) >> 10;
		if ((region == 5) || (region == 6)) region ^= 3;
		int selectedBank = bankSelect[region];
		int newBank = (selectedBank & ~0xFF) | value;
		if (newBank != selectedBank) {
			bankSelect[region] = newBank;
			setRom(region, newBank);
		}
	} else if (address == 0x7FF8) {
		// set mapper state (9th bit)
		for (int region = 0; region < 8; region++) {
			if (value & 1) {
				if (!(bankSelect[region] & 0x100)) {
					bankSelect[region] |= 0x100;
					setRom(region, bankSelect[region]);
				}
			} else {
				if (bankSelect[region] & 0x100) {
					bankSelect[region] &= ~0x100;
					setRom(region, bankSelect[region]);
				}
			}
			value >>= 1;
		}
	} else if (address == 0x7FF9) {
		// write control byte
		control = value;
	} else if ((0x8000 <= address) && (address < 0xC000)) {
		// SRAM TODO check
		int region = (address & 0x1C00) >> 10;
		int selectedBank = bankSelect[region];
		if ((0x80 <= selectedBank) && (selectedBank < 0x84)) {
			bank[address>>13][address&0x1FFF] = value;
		}
	}
}

byte* RomPanasonic::getWriteCacheLine(word address) const
{
	if ((0x6000 <= address) && (address < 0x7FF0)) {
		return NULL;
	} else if (address == (0x7FF8 & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	} else if ((0x8000 <= address) && (address < 0xC000)) {
		int region = (address & 0x1C00) >> 10;
		int selectedBank = bankSelect[region];
		if ((0x80 <= selectedBank) && (selectedBank < 0x84)) {
			return &bank[address>>13][address&0x1FFF];
		} else {
			return unmappedWrite;
		}
	} else {
		return unmappedWrite;
	}
}
