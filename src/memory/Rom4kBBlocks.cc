// $Id$

#include "Rom4kBBlocks.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

Rom4kBBlocks::Rom4kBBlocks(
		MSXMotherBoard& motherBoard, const XMLElement& config,
		std::auto_ptr<Rom> rom)
	: MSXRom(motherBoard, config, rom)
{
	for (int i = 0; i < 16; i++) {
		setRom(i, 0);
	}
}

byte Rom4kBBlocks::readMem(word address, const EmuTime& /*time*/)
{
	return bank[address >> 12][address & 0x0FFF];
}

const byte* Rom4kBBlocks::getReadCacheLine(word address) const
{
	return &bank[address >> 12][address & 0x0FFF];
}

void Rom4kBBlocks::setBank(byte region, const byte* adr)
{
	bank[region] = adr;
	invalidateMemCache(region * 0x1000, 0x1000);
}

void Rom4kBBlocks::setRom(byte region, int block)
{
	int nrBlocks = rom->getSize() >> 12;
	if (nrBlocks != 0) {
		block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
		setBank(region, &(*rom)[block << 12]);
	} else {
		setBank(region, unmappedRead);
	}
}

template<typename Archive>
void Rom4kBBlocks::serialize(Archive& ar, unsigned /*version*/)
{
	// TODO this is very similar to Rom8kBBlocks::serialize(),
	//      move this to a common place

	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	int offsets[16];
	if (ar.isLoader()) {
		ar.serialize("banks", offsets);
		for (int i = 0; i < 16; ++i) {
			// TODO SRAM
			bank[i] = (offsets[i] == -1)
			        ? unmappedRead
			        : &(*rom)[offsets[i]];
		}
	} else {
		for (int i = 0; i < 16; ++i) {
			// TODO SRAM
			offsets[i] = (bank[i] == unmappedRead)
			           ? -1
			           : (bank[i] - &(*rom)[0]);
		}
		ar.serialize("banks", offsets);
	}
}
INSTANTIATE_SERIALIZE_METHODS(Rom4kBBlocks);

} // namespace openmsx
