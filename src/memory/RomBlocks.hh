// $Id$

#ifndef ROMBLOCKS_HH
#define ROMBLOCKS_HH

#include "MSXRom.hh"
#include "serialize_meta.hh"

namespace openmsx {

class SRAM;
template<unsigned> class RomBlockDebuggable;

template <unsigned BANK_SIZE>
class RomBlocks : public MSXRom
{
	static const unsigned NUM_BANKS = 0x10000 / BANK_SIZE;
	static const unsigned BANK_MASK = BANK_SIZE - 1;
public:
	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	RomBlocks(MSXMotherBoard& motherBoard, const XMLElement& config,
	          std::auto_ptr<Rom> rom);
	~RomBlocks();

	/** Select 'unmapped' memory for this region. Reads from this
	  * region will return 0xff.
	  */
	void setUnmapped(byte region);

	/** Sets the memory visible for reading in a certain region.
	  * @param region number of 8kB region in Z80 address space
	  *   (region i starts at Z80 address i * 0x2000)
	  * @param adr pointer to memory, area must be at least 0x2000 bytes long
	  * @param block Block number, only used for the 'romblock' debuggable.
	  */
	void setBank(byte region, const byte* adr, int block);

	/** Selects a block of the ROM image for reading in a certain region.
	  * @param region number of 8kB region in Z80 address space
	  *   (region i starts at Z80 address i * 0x2000)
	  * @param block number of 8kB block in the ROM image
	  *   (block i starts at ROM image offset i * 0x2000)
	  */
	void setRom(byte region, int block);

	/** Sets a mask for the block numbers.
	  * On every call to setRom, the given block number is AND-ed with this
	  * mask; if the resulting number if outside of the ROM, unmapped memory
	  * is selected.
	  * By default the mask is set up to wrap at the end of the ROM image,
	  * meaning the entire ROM is reachable and there is no unmapped memory.
	  */
	void setBlockMask(int mask);

	/** Inform this base class of extra mapable memory block.
	 * This is needed for serialization of mappings in this block.
	 * Should only be called from subclass constructor.
	 * (e.g. used by RomPanasonic)
	 */
	void setExtraMemory(const byte* mem, unsigned size);

	const byte* bank[NUM_BANKS];
	std::auto_ptr<SRAM> sram; // can be a NULL ptr
	byte blockNr[NUM_BANKS];

private:
	const std::auto_ptr<RomBlockDebuggable<BANK_SIZE> > romBlockDebug;
	const byte* extraMem;
	unsigned extraSize;
	const int nrBlocks;
	int blockMask;

	friend class RomBlockDebuggable<BANK_SIZE>;
};

typedef RomBlocks<0x1000> Rom4kBBlocks;
typedef RomBlocks<0x2000> Rom8kBBlocks;
typedef RomBlocks<0x4000> Rom16kBBlocks;

REGISTER_BASE_CLASS(Rom4kBBlocks,  "Rom4kBBlocks");
REGISTER_BASE_CLASS(Rom8kBBlocks,  "Rom8kBBlocks");
REGISTER_BASE_CLASS(Rom16kBBlocks, "Rom16kBBlocks");

// TODO see comment in .cc
//SERIALIZE_CLASS_VERSION(Rom4kBBlocks,  2);
//SERIALIZE_CLASS_VERSION(Rom8kBBlocks,  2);
//SERIALIZE_CLASS_VERSION(Rom16kBBlocks, 2);

} // namespace openmsx

#endif
