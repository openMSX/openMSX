// $Id$

#ifndef __ROMKOREAN126IN1_HH__
#define __ROMKOREAN126IN1_HH__

#include "Rom16kBBlocks.hh"

namespace openmsx {

class RomKorean126in1 : public Rom16kBBlocks
{
public:
	RomKorean126in1(Config* config, const EmuTime& time, Rom* rom);
	virtual ~RomKorean126in1();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
