#ifndef ROMBLOCKS_HH
#define ROMBLOCKS_HH

#include "MSXRom.hh"
#include "RomBlockDebuggable.hh"
#include "serialize_meta.hh"
#include <array>

namespace openmsx {

class SRAM;

template<unsigned BANK_SIZE_>
class RomBlocks : public MSXRom
{
public:
	static constexpr unsigned BANK_SIZE = BANK_SIZE_;
	static constexpr unsigned NUM_BANKS = 0x10000 / BANK_SIZE;
	static constexpr unsigned BANK_MASK = BANK_SIZE - 1;

	[[nodiscard]] unsigned getBaseSizeAlignment() const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	/** Constructor
	 * @param config
	 * @param rom
	 * @param debugBankSizeShift Sometimes the mapper is implemented with a
	 *      smaller block size than the blocks that get logically switched.
	 *      For example RomGameMaster2 is implemented as a 4kB mapper but
	 *      blocks get switched in 8kB chunks (done like this because there
	 *      is only 4kB SRAM, that needs to be mirrored in a 8kB chunk).
	 *      This parameter indicates how much bigger the logical blocks are
	 *      compared to the implementation block size, it's only used to
	 *      correctly implement the 'romblocks' debuggable.
	 */
	RomBlocks(const DeviceConfig& config, Rom&& rom,
	          unsigned debugBankSizeShift = 0);
	~RomBlocks() override;

	/** Select 'unmapped' memory for this region. Reads from this
	  * region will return 0xff.
	  */
	void setUnmapped(unsigned region);

	/** Sets the memory visible for reading in a certain region.
	  * @param region number of 8kB region in Z80 address space
	  *   (region i starts at Z80 address i * 0x2000)
	  * @param adr pointer to memory, area must be at least 0x2000 bytes long
	  * @param block Block number, only used for the 'romblock' debuggable, limited to 8-bit.
	  */
	void setBank(unsigned region, const byte* adr, byte block);

	/** Selects a block of the ROM image for reading in a certain region.
	  * @param region number of 8kB region in Z80 address space
	  *   (region i starts at Z80 address i * 0x2000)
	  * @param block number of 8kB block in the ROM image
	  *   (block i starts at ROM image offset i * 0x2000)
	  */
	void setRom(unsigned region, unsigned block);

	/** Sets a mask for the block numbers.
	  * On every call to setRom, the given block number is AND-ed with this
	  * mask; if the resulting number if outside of the ROM, unmapped memory
	  * is selected.
	  * By default the mask is set up to wrap at the end of the ROM image,
	  * meaning the entire ROM is reachable and there is no unmapped memory.
	  */
	void setBlockMask(int mask) { blockMask = mask; }

	/** Inform this base class of extra mappable memory block.
	 * This is needed for serialization of mappings in this block.
	 * Should only be called from subclass constructor.
	 * (e.g. used by RomPanasonic)
	 */
	void setExtraMemory(std::span<const byte> mem);

protected:
	std::array<const byte*, NUM_BANKS> bankPtr;
	std::unique_ptr<SRAM> sram; // can be nullptr
	std::array<byte, NUM_BANKS> blockNr;

private:
	RomBlockDebuggable romBlockDebug;
	std::span<const byte> extraMem;
	/*const*/ unsigned nrBlocks;
	int blockMask;
};

using Rom4kBBlocks  = RomBlocks<0x1000>;
using Rom8kBBlocks  = RomBlocks<0x2000>;
using Rom16kBBlocks = RomBlocks<0x4000>;

REGISTER_BASE_CLASS(Rom4kBBlocks,  "Rom4kBBlocks");
REGISTER_BASE_CLASS(Rom8kBBlocks,  "Rom8kBBlocks");
REGISTER_BASE_CLASS(Rom16kBBlocks, "Rom16kBBlocks");

// TODO see comment in .cc
//SERIALIZE_CLASS_VERSION(Rom4kBBlocks,  2);
//SERIALIZE_CLASS_VERSION(Rom8kBBlocks,  2);
//SERIALIZE_CLASS_VERSION(Rom16kBBlocks, 2);

} // namespace openmsx

#endif
