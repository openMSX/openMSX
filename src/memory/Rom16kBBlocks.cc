// $Id$

#include "Rom16kBBlocks.hh"
#include "MSXCPU.hh"
#include "Rom.hh"

namespace openmsx {

Rom16kBBlocks::Rom16kBBlocks(
	MSXMotherBoard& motherBoard, const XMLElement& config,
	const EmuTime& time, std::auto_ptr<Rom> rom)
	: MSXRom(motherBoard, config, time, rom)
{
	for (int i = 0; i < 4; i++) {
		setRom(i, 0);
	}
}

Rom16kBBlocks::~Rom16kBBlocks()
{
}

byte Rom16kBBlocks::readMem(word address, const EmuTime& /*time*/)
{
	return bank[address >> 14][address & 0x3FFF];
}

const byte* Rom16kBBlocks::getReadCacheLine(word address) const
{
	return &bank[address >> 14][address & 0x3FFF];
}

void Rom16kBBlocks::setBank(byte region, const byte* adr)
{
	bank[region] = adr;
	cpu.invalidateMemCache(region * 0x4000, 0x4000);
}

void Rom16kBBlocks::setRom(byte region, int block)
{
	int nrBlocks = rom->getSize() >> 14;
	if (nrBlocks != 0) {
		block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
		setBank(region, &(*rom)[block << 14]);
	} else {
		setBank(region, unmappedRead);
	}
}

} // namespace openmsx
