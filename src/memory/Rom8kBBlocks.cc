// $Id$

#include "Rom8kBBlocks.hh"
#include "MSXCPU.hh"
#include "CPU.hh"


Rom8kBBlocks::Rom8kBBlocks(Device* config, const EmuTime &time)
	: MSXDevice(config, time), MSXRom(config, time)
{
	for (int i = 0; i < 8; i++) {
		setRom(i, 0);
	}
}

Rom8kBBlocks::~Rom8kBBlocks()
{
}

byte Rom8kBBlocks::readMem(word address, const EmuTime &time)
{
	return bank[address >> 13][address & 0x1FFF];
}

const byte* Rom8kBBlocks::getReadCacheLine(word address) const
{
	return &bank[address >> 13][address & 0x1FFF];
}

void Rom8kBBlocks::setBank(byte region, byte* adr)
{
	bank[region] = adr;
	cpu->invalidateCache(region * 0x2000, 0x2000 / CPU::CACHE_LINE_SIZE);
}

void Rom8kBBlocks::setRom(byte region, int block)
{
	int nrBlocks = rom.getSize() >> 13;
	if (nrBlocks != 0) {
		block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
		setBank(region, const_cast<byte*>(rom.getBlock(block << 13)));
	} else {
		setBank(region, unmapped);
	}
}
