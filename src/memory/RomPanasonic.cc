// $Id$

#include "RomPanasonic.hh"
#include "PanasonicMemory.hh"
#include "MSXConfig.hh"
#include "SRAM.hh"
#include "CPU.hh"


const int SRAM_BASE = 0x80;
const int RAM_BASE  = 0x180;


RomPanasonic::RomPanasonic(Device* config, const EmuTime &time, Rom *rom)
	: MSXDevice(config, time), Rom8kBBlocks(config, time, rom)
{
	PanasonicMemory::instance()->registerRom(rom->getBlock(), rom->getSize());
	
	int sramSize;
	if (config->hasParameter("sramsize")) {
		sramSize = config->getParameterAsInt("sramsize");
		sram = new SRAM(sramSize * 1024, config);
	} else {
		sramSize = 0;
		sram = NULL;
	}

	bool sramMirrored;
	if (config->hasParameter("sram-mirrored")) {
		sramMirrored = config->getParameterAsBool("sram-mirrored");
	} else {
		sramMirrored = false;
	}
	
	if (sramMirrored) {
		maxSRAMBank = SRAM_BASE + 8;
	} else {
		maxSRAMBank = SRAM_BASE + (sramSize / 8);
	}
	
	reset(time);
}

RomPanasonic::~RomPanasonic()
{
	delete sram;
}

void RomPanasonic::reset(const EmuTime &time)
{
	control = 0;
	for (int region = 0; region < 8; region++) {
		bankSelect[region] = 0;
		setRom(region, 0);
	}
}

byte RomPanasonic::readMem(word address, const EmuTime &time)
{
	byte result;
	if ((control & 0x04) && (0x7FF0 <= address) && (address < 0x7FF7)) {
		// read mapper state (lower 8 bit)
		result = bankSelect[address & 7] & 0xFF;
	} else if ((control & 0x10) && (address == 0x7FF8)) {
		// read mapper state (9th bit)
		result = 0;
		for (int i = 7; i >= 0; i--) {
			result <<= 1;
			if (bankSelect[i] & 0x100) {
				result++;
			}
		}
	} else if ((control & 0x08) && (address == 0x7FF9)) {
		// read control byte
		result = control;
	} else {
		result = Rom8kBBlocks::readMem(address, time);
	}
	//PRT_DEBUG("DEBUG read "<<std::hex<<(int)address<<" "<<
	//          (int)result<<std::dec);
	return result;
}

const byte* RomPanasonic::getReadCacheLine(word address) const
{
	//return NULL;
	if ((0x7FF0 & CPU::CACHE_LINE_HIGH) == address) {
		// TODO check mirrored
		return NULL;
	} else {
		return Rom8kBBlocks::getReadCacheLine(address);
	}
}

void RomPanasonic::writeMem(word address, byte value, const EmuTime &time)
{
	//PRT_DEBUG("DEBUG write "<<std::hex<<(int)address<<
	//          " "<<(int)value<<std::dec);
	if ((0x6000 <= address) && (address < 0x7FF0)) {
		// set mapper state (lower 8 bits)
		int region = (address & 0x1C00) >> 10;
		if ((region == 5) || (region == 6)) region ^= 3;
		int selectedBank = bankSelect[region];
		int newBank = (selectedBank & ~0xFF) | value;
		changeBank(region, newBank);
	} else if (address == 0x7FF8) {
		// set mapper state (9th bit)
		for (int region = 0; region < 8; region++) {
			if (value & 1) {
				changeBank(region, bankSelect[region] |  0x100);
			} else {
				changeBank(region, bankSelect[region] & ~0x100);
			}
			value >>= 1;
		}
	} else if (address == 0x7FF9) {
		// write control byte
		control = value;
	} else if ((0x8000 <= address) && (address < 0xC000)) {
		int region = address >> 13;
		int selectedBank = bankSelect[region];
		if (sram && (SRAM_BASE <= selectedBank) &&
			    (selectedBank < maxSRAMBank)) {
			// SRAM
			bank[region][address & 0x1FFF] = value;
		} else if (RAM_BASE <= selectedBank) {
			// RAM
			bank[region][address & 0x1FFF] = value;
		}
	} 
}

byte* RomPanasonic::getWriteCacheLine(word address) const
{
	//return NULL;
	if ((0x6000 <= address) && (address < 0x7FF0)) {
		return NULL;
	} else if (address == (0x7FF8 & CPU::CACHE_LINE_HIGH)) {
		return NULL;
	} else if ((0x8000 <= address) && (address < 0xC000)) {
		int region = address >> 13;
		int selectedBank = bankSelect[region];
		if ((sram && (SRAM_BASE <= selectedBank) &&
			     (selectedBank < maxSRAMBank)) ||
		    (RAM_BASE <= selectedBank)) {
			return &bank[region][address & 0x1FFF];
		} else {
			return unmappedWrite;
		}
	} else {
		return unmappedWrite;
	}
}

void RomPanasonic::changeBank(byte region, int bank)
{
	if (bank == bankSelect[region]) {
		return;
	}
	bankSelect[region] = bank;
	if (sram && (SRAM_BASE <= bank) && (bank < maxSRAMBank)) {
		// SRAM
		int offset = (bank - SRAM_BASE) * 0x2000;
		int sramSize = sram->getSize();
		if (offset >= sramSize) {
			offset &= (sramSize - 1);
		}
		setBank(region, sram->getBlock(offset));
	} else if (RAM_BASE <= bank) {
		// RAM
		setBank(region, PanasonicMemory::instance()->
			getRamBlock(bank - RAM_BASE));
	} else {
		// ROM
		setRom(region, bank);
	}
}
