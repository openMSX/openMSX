// $Id$

#ifndef __ROMHOLYQURAN_HH__
#define __ROMHOLYQURAN_HH__

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomHolyQuran : public Rom8kBBlocks
{
public:
	RomHolyQuran(Config* config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~RomHolyQuran();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
