// $Id$

#include "RomBlocks.hh"
#include "Rom.hh"
#include "SRAM.hh"
#include "SimpleDebuggable.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

template <unsigned BANK_SIZE>
class RomBlockDebuggable : public SimpleDebuggable
{
public:
	explicit RomBlockDebuggable(RomBlocks<BANK_SIZE>& rom_)
		: SimpleDebuggable(rom_.getMotherBoard(),
		                   rom_.getName() + " romblocks",
		                   "Shows for each byte of the mapper which memory block is selected.",
		                   0x10000)
		, rom(rom_)
	{
	}

	virtual byte read(unsigned address)
	{
		return rom.blockNr[address / BANK_SIZE];
	}
private:
	RomBlocks<BANK_SIZE>& rom;
};


template <unsigned BANK_SIZE>
RomBlocks<BANK_SIZE>::RomBlocks(
		MSXMotherBoard& motherBoard, const XMLElement& config,
		std::auto_ptr<Rom> rom_)
	: MSXRom(motherBoard, config, rom_)
	, romBlockDebug(new RomBlockDebuggable<BANK_SIZE>(*this))
	, nrBlocks(rom->getSize() / BANK_SIZE)
{
	if ((nrBlocks * BANK_SIZE) != rom->getSize()) {
		throw MSXException(StringOp::Builder() <<
			"(uncompressed) ROM image filesize must be a multiple of " <<
			BANK_SIZE / 1024 << " kB (for this mapper type).");
	}
	// by default no extra mappable memory block
	extraMem = NULL;
	extraSize = 0;

	// Default mask: wraps at end of ROM image.
	blockMask = nrBlocks - 1;
	for (unsigned i = 0; i < NUM_BANKS; i++) {
		setRom(i, 0);
	}
}

template <unsigned BANK_SIZE>
RomBlocks<BANK_SIZE>::~RomBlocks()
{
}

template <unsigned BANK_SIZE>
byte RomBlocks<BANK_SIZE>::readMem(word address, EmuTime::param /*time*/)
{
	return bank[address / BANK_SIZE][address & BANK_MASK];
}

template <unsigned BANK_SIZE>
const byte* RomBlocks<BANK_SIZE>::getReadCacheLine(word address) const
{
	return &bank[address / BANK_SIZE][address & BANK_MASK];
}

template <unsigned BANK_SIZE>
void RomBlocks<BANK_SIZE>::setBank(byte region, const byte* adr, int block)
{
	assert("address passed to setBank() is not serializable" &&
	       ((adr == unmappedRead) ||
	        ((&(*rom)[0] <= adr) && (adr <= &(*rom)[rom->getSize() - 1])) ||
	        (sram.get() && (&(*sram)[0] <= adr) &&
	                       (adr <= &(*sram)[sram->getSize() - 1])) ||
	        ((extraMem <= adr) && (adr <= &extraMem[extraSize - 1]))));
	bank[region] = adr;
	blockNr[region] = block; // only for debuggable
	invalidateMemCache(region * BANK_SIZE, BANK_SIZE);
}

template <unsigned BANK_SIZE>
void RomBlocks<BANK_SIZE>::setUnmapped(byte region)
{
	setBank(region, unmappedRead, 255);
}

template <unsigned BANK_SIZE>
void RomBlocks<BANK_SIZE>::setBlockMask(int mask)
{
	blockMask = mask;
}

template <unsigned BANK_SIZE>
void RomBlocks<BANK_SIZE>::setExtraMemory(const byte* mem, unsigned size)
{
	extraMem = mem;
	extraSize = size;
}

template <unsigned BANK_SIZE>
void RomBlocks<BANK_SIZE>::setRom(byte region, int block)
{
	// Note: Some cartridges have a number of blocks that is not a power of 2,
	//       for those we have to make an exception for "block < nrBlocks".
	block = (block < nrBlocks) ? block : block & blockMask;
	if (block < nrBlocks) {
		setBank(region, &(*rom)[block * BANK_SIZE], block);
	} else {
		setBank(region, unmappedRead, 255);
	}
}

// version 1: initial version
// version 2: added blockNr
template <unsigned BANK_SIZE>
template<typename Archive>
void RomBlocks<BANK_SIZE>::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	if (sram.get()) {
		ar.serialize("sram", *sram);
	}

	unsigned offsets[NUM_BANKS];
	unsigned romSize = rom->getSize();
	unsigned sramSize = sram.get() ? sram->getSize() : 0;
	if (ar.isLoader()) {
		ar.serialize("banks", offsets);
		for (unsigned i = 0; i < NUM_BANKS; ++i) {
			if (offsets[i] == unsigned(-1)) {
				bank[i] = unmappedRead;
			} else if (offsets[i] < romSize) {
				bank[i] = &(*rom)[offsets[i]];
			} else if (offsets[i] < (romSize + sramSize)) {
				assert(sram.get());
				bank[i] = &(*sram)[offsets[i] - romSize];
			} else if (offsets[i] < (romSize + sramSize + extraSize)) {
				bank[i] = &extraMem[offsets[i] - romSize - sramSize];
			} else {
				// TODO throw
				UNREACHABLE;
			}
		}
	} else {
		for (unsigned i = 0; i < NUM_BANKS; ++i) {
			if (bank[i] == unmappedRead) {
				offsets[i] = unsigned(-1);
			} else if ((&(*rom)[0] <= bank[i]) &&
			           (bank[i] <= &(*rom)[romSize - 1])) {
				offsets[i] = unsigned(bank[i] - &(*rom)[0]);
			} else if (sram.get() &&
			           (&(*sram)[0] <= bank[i]) &&
			           (bank[i] <= &(*sram)[sramSize - 1])) {
				offsets[i] = unsigned(bank[i] - &(*sram)[0] + romSize);
			} else if ((extraMem <= bank[i]) &&
			           (bank[i] <= &extraMem[extraSize - 1])) {
				offsets[i] = unsigned(bank[i] - extraMem + romSize + sramSize);
			} else {
				UNREACHABLE;
			}
		}
		ar.serialize("banks", offsets);
	}

	// Commented out because versioning doesn't work correct on subclasses
	// that don't override the serialize() method (e.g. RomPlain)
	/*if (version >= 2) {
		ar.serialize("blockNr", blockNr);
	} else {
		assert(ar.isLoader());
		// set dummy value, anyway only used for debuggable
		for (unsigned i = 0; i < NUM_BANKS; ++i) {
			blockNr[i] = 255;
		}
	}*/
}

template class RomBlocks<0x1000>;
template class RomBlocks<0x2000>;
template class RomBlocks<0x4000>;
INSTANTIATE_SERIALIZE_METHODS(Rom4kBBlocks);
INSTANTIATE_SERIALIZE_METHODS(Rom8kBBlocks);
INSTANTIATE_SERIALIZE_METHODS(Rom16kBBlocks);

} // namespace openmsx
