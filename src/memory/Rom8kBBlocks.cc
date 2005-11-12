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

void Rom8kBBlocks::setRom(byte region, int block)
{
	int nrBlocks = rom->getSize() >> 13;
	if (nrBlocks != 0) {
		block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
		setBank(region, &(*rom)[block << 13]);
	} else {
		setBank(region, unmappedRead);
	}
}

} // namespace openmsx
