// $Id$

#ifndef __ROMKOEI_HH__
#define __ROMKOEI_HH__

#include "Rom8kBBlocks.hh"
#include "SRAM.hh"

namespace openmsx {

class RomKoei : public Rom8kBBlocks
{
public:
	enum SramSize { SRAM8 = 1, SRAM32 = 4 };
	RomKoei(Config* config, const EmuTime& time, Rom* rom, SramSize sramSize);
	virtual ~RomKoei();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	SRAM sram;
	byte sramEnabled;
	byte sramBlock[8];
};

} // namespace openmsx

#endif
