// $Id$

#ifndef ROM16KBBLOCKS_HH
#define ROM16KBBLOCKS_HH

#include "MSXRom.hh"

namespace openmsx {

class Rom16kBBlocks : public MSXRom
{
public:
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

protected:
	Rom16kBBlocks(MSXMotherBoard& motherBoard, const XMLElement& config,
	              const EmuTime& time, std::auto_ptr<Rom> rom);

	void setBank(byte region, const byte* adr);
	void setRom(byte region, int block);

	const byte* bank[4];
};

} // namespace openmsx

#endif
