// $Id$

#ifndef __ROMHALNOTE_HH__
#define __ROMHALNOTE_HH__

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomHalnote : public Rom8kBBlocks
{
public:
	RomHalnote(Config* config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~RomHalnote();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
