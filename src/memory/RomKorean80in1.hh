// $Id$

#ifndef __ROMKOREAN80IN1_HH__
#define __ROMKOREAN80IN1_HH__

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomKorean80in1 : public Rom8kBBlocks
{
public:
	RomKorean80in1(Config* config, const EmuTime& time, Rom* rom);
	virtual ~RomKorean80in1();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
