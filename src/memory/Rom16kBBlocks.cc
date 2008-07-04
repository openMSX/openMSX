// $Id$

#include "Rom16kBBlocks.hh"
#include "Rom.hh"
#include "serialize.hh"

namespace openmsx {

Rom16kBBlocks::Rom16kBBlocks(
		MSXMotherBoard& motherBoard, const XMLElement& config,
		std::auto_ptr<Rom> rom)
	: MSXRom(motherBoard, config, rom)
{
	for (int i = 0; i < 4; i++) {
		setRom(i, 0);
	}
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
	invalidateMemCache(region * 0x4000, 0x4000);
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

template<typename Archive>
void Rom16kBBlocks::serialize(Archive& ar, unsigned /*version*/)
{
	// TODO this is very similar to Rom8kBBlocks::serialize(),
	//      move this to a common place

	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	int offsets[4];
	if (ar.isLoader()) {
		ar.serialize("banks", offsets);
		for (int i = 0; i < 4; ++i) {
			// TODO SRAM
			bank[i] = (offsets[i] == -1)
			        ? unmappedRead
			        : &(*rom)[offsets[i]];
		}
	} else {
		for (int i = 0; i < 4; ++i) {
			// TODO SRAM
			offsets[i] = (bank[i] == unmappedRead)
			           ? -1
			           : (bank[i] - &(*rom)[0]);
		}
		ar.serialize("banks", offsets);
	}
}
INSTANTIATE_SERIALIZE_METHODS(Rom16kBBlocks);

} // namespace openmsx
