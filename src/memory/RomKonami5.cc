// $Id$

// KONAMI5 8kB cartridges
// 
// this type is used by Konami cartridges that do have an SCC and some others
// examples of cartridges: Nemesis 2, Nemesis 3, King's Valley 2, Space Manbow
// Solid Snake, Quarth, Ashguine 1, Animal, Arkanoid 2, ...
// Those last 3 were probably modified ROM images, they should be ASCII8
//
// The address to change banks:
//  bank 1: 0x5000 - 0x57ff (0x5000 used)
//  bank 2: 0x7000 - 0x77ff (0x7000 used)
//  bank 3: 0x9000 - 0x97ff (0x9000 used)
//  bank 4: 0xB000 - 0xB7ff (0xB000 used)

#include "RomKonami5.hh"
#include "MSXConfig.hh"
#include "SCC.hh"
#include "MSXCPU.hh"
#include "CPU.hh"


RomKonami5::RomKonami5(Device* config, const EmuTime &time, Rom *rom)
	: MSXDevice(config, time), Rom8kBBlocks(config, time, rom)
{
	short volume = (short)config->getParameterAsInt("volume");
	scc = new SCC(volume, time);
	
	reset(time);
}

RomKonami5::~RomKonami5()
{
	delete scc;
}

void RomKonami5::reset(const EmuTime &time)
{
	setBank(0, unmappedRead);
	setBank(1, unmappedRead);
	for (int i = 2; i < 6; i++) {
		setRom(i, i - 2);
	}
	setBank(6, unmappedRead);
	setBank(7, unmappedRead);

	sccEnabled = false;
	scc->reset(time);
}

byte RomKonami5::readMem(word address, const EmuTime &time)
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		return scc->readMemInterface(address & 0xFF, time);
	} else {
		return Rom8kBBlocks::readMem(address, time);
	}
}

const byte* RomKonami5::getReadCacheLine(word address) const
{
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		// don't cache SCC
		return NULL;
	} else {
		return Rom8kBBlocks::getReadCacheLine(address);
	}
}

void RomKonami5::writeMem(word address, byte value, const EmuTime &time)
{
	if ((address < 0x5000) || (address >= 0xC000)) {
		return;
	}
	if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		// write to SCC
		scc->writeMemInterface(address & 0xFF, value, time);
		return;
	}
	if ((address & 0xF800) == 0x9000) {
		// SCC enable/disable
		sccEnabled = ((value & 0x3F) == 0x3F);
		cpu->invalidateCache(0x9800, 0x0800/CPU::CACHE_LINE_SIZE);
	}
	if ((address & 0x1800) == 0x1000) {
		// page selection
		setRom(address >> 13, value);
	}
}

byte* RomKonami5::getWriteCacheLine(word address) const
{
	if ((address < 0x5000) || (address >= 0xC000)) {
		return unmappedWrite;
	} else if (sccEnabled && (0x9800 <= address) && (address < 0xA000)) {
		// write to SCC
		return NULL;
	} else if ((address & 0xF800) == (0x9000 & CPU::CACHE_LINE_HIGH)) {
		// SCC enable/disable
		return NULL;
	} else if ((address & 0x1800) == (0x1000 & CPU::CACHE_LINE_HIGH)) {
		// page selection
		return NULL;
	} else {
		return unmappedWrite;
	}
}
