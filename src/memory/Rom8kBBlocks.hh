// $Id$

#ifndef __ROM8KBBLOCKS_HH__
#define __ROM8KBBLOCKS_HH__

#include "MSXRom.hh"

namespace openmsx {

class Rom8kBBlocks : public MSXRom
{
public:
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

protected:
	Rom8kBBlocks(Config* config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~Rom8kBBlocks();

	void setBank(byte region, byte* adr);
	void setRom(byte region, int block);

	byte* bank[8];
};

} // namespace openmsx

#endif
