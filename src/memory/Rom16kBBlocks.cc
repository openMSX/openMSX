// $Id$

#include "Rom16kBBlocks.hh"
#include "MSXCPU.hh"
#include "CPU.hh"


namespace openmsx {

Rom16kBBlocks::Rom16kBBlocks(Device* config, const EmuTime &time, Rom *rom)
	: MSXDevice(config, time), MSXRom(config, time, rom)
{
	for (int i = 0; i < 4; i++) {
		setRom(i, 0);
	}
}

Rom16kBBlocks::~Rom16kBBlocks()
{
}

byte Rom16kBBlocks::readMem(word address, const EmuTime &time)
{
	return bank[address >> 14][address & 0x3FFF];
}

const byte* Rom16kBBlocks::getReadCacheLine(word address) const
{
	return &bank[address >> 14][address & 0x3FFF];
}

void Rom16kBBlocks::setBank(byte region, byte* adr)
{
	bank[region] = adr;
	cpu->invalidateCache(region * 0x4000, 0x4000 / CPU::CACHE_LINE_SIZE);
}

void Rom16kBBlocks::setRom(byte region, int block)
{
	int nrBlocks = rom->getSize() >> 14;
	if (nrBlocks != 0) {
		block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
		setBank(region, const_cast<byte*>(rom->getBlock(block << 14)));
	} else {
		setBank(region, unmappedRead);
	}
}

} // namespace openmsx
