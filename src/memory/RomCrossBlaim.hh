// $Id$

#ifndef __ROMCROSSBLAIM_HH__
#define __ROMCROSSBLAIM_HH__

#include "Rom16kBBlocks.hh"

namespace openmsx {

class RomCrossBlaim : public Rom16kBBlocks
{
public:
	RomCrossBlaim(Config* config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~RomCrossBlaim();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
