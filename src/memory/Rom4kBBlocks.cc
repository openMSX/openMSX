// $Id$

#include "Rom4kBBlocks.hh"
#include "MSXCPU.hh"
#include "CPU.hh"


Rom4kBBlocks::Rom4kBBlocks(Device* config, const EmuTime &time)
	: MSXDevice(config, time), MSXRom(config, time)
{
	for (int i = 0; i < 16; i++) {
		setRom(i, 0);
	}
}

Rom4kBBlocks::~Rom4kBBlocks()
{
}

byte Rom4kBBlocks::readMem(word address, const EmuTime &time)
{
	return bank[address >> 12][address & 0x0FFF];
}

const byte* Rom4kBBlocks::getReadCacheLine(word address) const
{
	return &bank[address >> 12][address & 0x0FFF];
}

void Rom4kBBlocks::setBank(byte region, byte* adr)
{
	bank[region] = adr;
	cpu->invalidateCache(region * 0x1000, 0x1000 / CPU::CACHE_LINE_SIZE);
}

void Rom4kBBlocks::setRom(byte region, int block)
{
	int nrBlocks = rom.getSize() >> 12;
	if (nrBlocks != 0) {
		block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
		setBank(region, const_cast<byte*>(rom.getBlock(block << 12)));
	} else {
		setBank(region, unmappedRead);
	}
}
