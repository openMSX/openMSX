#include "CliComm.hh"
#include "RomBlocks.hh"
#include "SRAM.hh"
#include "narrow.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <bit>

namespace openmsx {

// minimal attempt to avoid seeing this warning too often
static Sha1Sum alreadyWarnedForSha1Sum;

template<unsigned BANK_SIZE>
RomBlocks<BANK_SIZE>::RomBlocks(
		const DeviceConfig& config, Rom&& rom_,
		unsigned debugBankSizeShift)
	: MSXRom(config, std::move(rom_))
	, romBlockDebug(
		*this,  blockNr, 0x0000, 0x10000,
		std::bit_width(BANK_SIZE) - 1, debugBankSizeShift)
{
	static_assert(std::has_single_bit(BANK_SIZE), "BANK_SIZE must be a power of two");
	auto extendedSize = (rom.size() + BANK_SIZE - 1) & ~(BANK_SIZE - 1);
	if (extendedSize != rom.size() && alreadyWarnedForSha1Sum != rom.getOriginalSHA1()) {
		config.getCliComm().printWarning(
			"(uncompressed) ROM image filesize was not a multiple "
			"of ", BANK_SIZE / 1024, "kB (which is required for mapper type ",
			config.findChild("mappertype")->getData(), "), so we "
			"padded it to be correct. But if the ROM you are "
			"running was just dumped, the dump is probably not "
			"complete/correct!");
		alreadyWarnedForSha1Sum = rom.getOriginalSHA1();
	}
	rom.addPadding(extendedSize);
	nrBlocks = narrow<decltype(nrBlocks)>(rom.size() / BANK_SIZE);
	assert((nrBlocks * BANK_SIZE) == rom.size());

	// by default no extra mappable memory block
	extraMem = {};

	// Default mask: wraps at end of ROM image.
	blockMask = nrBlocks - 1;
	for (auto i : xrange(NUM_BANKS)) {
		setRom(i, 0);
	}
}

template<unsigned BANK_SIZE>
RomBlocks<BANK_SIZE>::~RomBlocks() = default;

template<unsigned BANK_SIZE>
unsigned RomBlocks<BANK_SIZE>::getBaseSizeAlignment() const
{
	return BANK_SIZE;
}

template<unsigned BANK_SIZE>
byte RomBlocks<BANK_SIZE>::peekMem(word address, EmuTime::param /*time*/) const
{
	return bankPtr[address / BANK_SIZE][address & BANK_MASK];
}

template<unsigned BANK_SIZE>
byte RomBlocks<BANK_SIZE>::readMem(word address, EmuTime::param time)
{
	return RomBlocks<BANK_SIZE>::peekMem(address, time);
}

template<unsigned BANK_SIZE>
const byte* RomBlocks<BANK_SIZE>::getReadCacheLine(word address) const
{
	return &bankPtr[address / BANK_SIZE][address & BANK_MASK];
}

template<unsigned BANK_SIZE>
void RomBlocks<BANK_SIZE>::setBank(unsigned region, const byte* adr, byte block)
{
	assert("address passed to setBank() is not serializable" &&
	       ((adr == unmappedRead.data()) ||
	        ((&rom[0] <= adr) && (adr <= &rom[rom.size() - 1])) ||
	        (sram && (&(*sram)[0] <= adr) &&
	                       (adr <= &(*sram)[sram->size() - 1])) ||
	        (!extraMem.empty() && (&extraMem.front() <= adr) && (adr <= &extraMem.back()))));
	bankPtr[region] = adr;
	blockNr[region] = block; // only for debuggable
	fillDeviceRCache(region * BANK_SIZE, BANK_SIZE, adr);
}

template<unsigned BANK_SIZE>
void RomBlocks<BANK_SIZE>::setUnmapped(unsigned region)
{
	setBank(region, unmappedRead.data(), 255);
}

template<unsigned BANK_SIZE>
void RomBlocks<BANK_SIZE>::setExtraMemory(std::span<const byte> mem)
{
	extraMem = mem;
}

template<unsigned BANK_SIZE>
void RomBlocks<BANK_SIZE>::setRom(unsigned region, unsigned block)
{
	// Note: Some cartridges have a number of blocks that is not a power of 2,
	//       for those we have to make an exception for "block < nrBlocks".
	block = (block < nrBlocks) ? block : block & blockMask;
	if (block < nrBlocks) {
		setBank(region, &rom[block * BANK_SIZE],
		        narrow_cast<byte>(block)); // only used for debug, narrowing is fine
	} else {
		setBank(region, unmappedRead.data(), 255);
	}
}

// version 1: initial version
// version 2: added blockNr
template<unsigned BANK_SIZE>
template<typename Archive>
void RomBlocks<BANK_SIZE>::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	if (sram) ar.serialize("sram", *sram);

	std::array<size_t, NUM_BANKS> offsets;
	auto romSize = rom.size();
	auto sramSize = sram ? sram->size() : 0;
	if constexpr (Archive::IS_LOADER) {
		ar.serialize("banks", offsets);
		for (auto i : xrange(NUM_BANKS)) {
			if (offsets[i] == size_t(-1)) {
				bankPtr[i] = unmappedRead.data();
			} else if (offsets[i] < romSize) {
				bankPtr[i] = &rom[offsets[i]];
			} else if (offsets[i] < (romSize + sramSize)) {
				assert(sram);
				bankPtr[i] = &(*sram)[offsets[i] - romSize];
			} else if (offsets[i] < (romSize + sramSize + extraMem.size())) {
				bankPtr[i] = &extraMem[offsets[i] - romSize - sramSize];
			} else {
				// TODO throw
				UNREACHABLE;
			}
		}
	} else {
		for (auto i : xrange(NUM_BANKS)) {
			if (bankPtr[i] == unmappedRead.data()) {
				offsets[i] = size_t(-1);
			} else if ((&rom[0] <= bankPtr[i]) &&
			           (bankPtr[i] <= &rom[romSize - 1])) {
				offsets[i] = size_t(bankPtr[i] - &rom[0]);
			} else if (sram && (&(*sram)[0] <= bankPtr[i]) &&
			           (bankPtr[i] <= &(*sram)[sramSize - 1])) {
				offsets[i] = size_t(bankPtr[i] - &(*sram)[0] + romSize);
			} else if (!extraMem.empty() &&
				   (&extraMem.front() <= bankPtr[i]) &&
			           (bankPtr[i] <= &extraMem.back())) {
				offsets[i] = size_t(bankPtr[i] - extraMem.data() + romSize + sramSize);
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
		assert(Archive::IS_LOADER);
		// set dummy value, anyway only used for debuggable
		ranges::fill(blockNr, 255);
	}*/
}

template class RomBlocks<0x1000>;
template class RomBlocks<0x2000>;
template class RomBlocks<0x4000>;
INSTANTIATE_SERIALIZE_METHODS(Rom4kBBlocks);
INSTANTIATE_SERIALIZE_METHODS(Rom8kBBlocks);
INSTANTIATE_SERIALIZE_METHODS(Rom16kBBlocks);

} // namespace openmsx
