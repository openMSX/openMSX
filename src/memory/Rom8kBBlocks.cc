// $Id$

#include "Rom8kBBlocks.hh"
#include "MSXCPU.hh"
#include "Rom.hh"

namespace openmsx {

Rom8kBBlocks::Rom8kBBlocks(
	MSXMotherBoard& motherBoard, const XMLElement& config,
	const EmuTime& time, std::auto_ptr<Rom> rom)
	: MSXRom(motherBoard, config, time, rom)
{
	// Note: Do not use the "rom" parameter, because an auto_ptr contains NULL
	//       after it is copied.
	nrBlocks = this->rom->getSize() >> 13;
	// Default mask: wraps at end of ROM image.
	blockMask = nrBlocks - 1;
	for (int i = 0; i < 8; i++) {
		setRom(i, 0);
	}
}

byte Rom8kBBlocks::readMem(word address, const EmuTime& /*time*/)
{
	return bank[address >> 13][address & 0x1FFF];
}

const byte* Rom8kBBlocks::getReadCacheLine(word address) const
{
	return &bank[address >> 13][address & 0x1FFF];
}

void Rom8kBBlocks::setBank(byte region, const byte* adr)
{
	bank[region] = adr;
	cpu.invalidateMemCache(region * 0x2000, 0x2000);
}

void Rom8kBBlocks::setBlockMask(int mask)
{
	blockMask = mask;
}

void Rom8kBBlocks::setRom(byte region, int block)
{
	// Note: Some cartridges have a number of blocks that is not a power of 2,
	//       for those we have to make an exception for "block < nrBlocks".
	block = (block < nrBlocks) ? block : block & blockMask;
	if (block < nrBlocks) {
		setBank(region, &(*rom)[block << 13]);
	} else {
		setBank(region, unmappedRead);
	}
}

} // namespace openmsx
