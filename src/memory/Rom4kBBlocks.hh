// $Id$

#ifndef ROM4KBBLOCKS_HH
#define ROM4KBBLOCKS_HH

#include "MSXRom.hh"

namespace openmsx {

class Rom4kBBlocks : public MSXRom
{
public:
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

protected:
	Rom4kBBlocks(MSXMotherBoard& motherBoard, const XMLElement& config,
	             const EmuTime& time, std::auto_ptr<Rom> rom);

	void setBank(byte region, const byte* adr);
	void setRom(byte region, int block);

	const byte* bank[16];
};

} // namespace openmsx

#endif
