// $Id$

#include "RomBlocks.hh"
#include "RomBlockDebuggable.hh"
#include "Rom.hh"
#include "SRAM.hh"
#include "SimpleDebuggable.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

template<bool C, class T, class F> struct if_log2_             : F {};
template<        class T, class F> struct if_log2_<true, T, F> : T {};
template<int I> struct int_ { static const int value = I; };
template<unsigned A, unsigned R = 0> struct log2
	: if_log2_<A == 1, int_<R>, log2<A / 2, R + 1> > {};

template <unsigned BANK_SIZE>
RomBlocks<BANK_SIZE>::RomBlocks(
		const DeviceConfig& config, std::unique_ptr<Rom> rom_,
		unsigned debugBankSizeShift)
	: MSXRom(config, std::move(rom_))
	, romBlockDebug(new RomBlockDebuggable(
		*this,  blockNr, 0x0000, 0x10000,
		log2<BANK_SIZE>::value, debugBankSizeShift))
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
	/*if (ar.versionAtLeast(version, 2)) {
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
