// $Id$

#ifndef __ROM16KBBLOCKS_HH__
#define __ROM16KBBLOCKS_HH__

#include "MSXRom.hh"

namespace openmsx {

class Rom16kBBlocks : public MSXRom
{
public:
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

protected:
	Rom16kBBlocks(const XMLElement& config, const EmuTime& time,
	              std::auto_ptr<Rom> rom);
	virtual ~Rom16kBBlocks();

	void setBank(byte region, byte* adr);
	void setRom(byte region, int block);

	byte* bank[4];
};

} // namespace openmsx

#endif
