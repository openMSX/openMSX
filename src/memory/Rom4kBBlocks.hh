// $Id$

#ifndef __ROM4KBBLOCKS_HH__
#define __ROM4KBBLOCKS_HH__

#include "MSXRom.hh"

namespace openmsx {

class Rom4kBBlocks : public MSXRom
{
public:
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;

protected:
	Rom4kBBlocks(const XMLElement& config, const EmuTime& time,
	             auto_ptr<Rom> rom);
	virtual ~Rom4kBBlocks();

	void setBank(byte region, byte* adr);
	void setRom(byte region, int block);

	byte* bank[16];
};

} // namespace openmsx

#endif
