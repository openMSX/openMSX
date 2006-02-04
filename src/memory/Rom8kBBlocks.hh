// $Id$

#ifndef ROM8KBBLOCKS_HH
#define ROM8KBBLOCKS_HH

#include "MSXRom.hh"

namespace openmsx {

class Rom8kBBlocks : public MSXRom
{
public:
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

protected:
	Rom8kBBlocks(MSXMotherBoard& motherBoard, const XMLElement& config,
	             const EmuTime& time, std::auto_ptr<Rom> rom);

	/** Sets the memory visible for reading in a certain region.
	  * @param region number of 8kB region in Z80 address space
	  *   (region i starts at Z80 address i * 0x2000)
	  * @param adr pointer to memory, area must be at least 0x2000 bytes long
	  */
	void setBank(byte region, const byte* adr);

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

	const byte* bank[8];

private:
	int nrBlocks;
	int blockMask;
};

} // namespace openmsx

#endif
