// $Id$

#ifndef __ROMKONAMI4_HH__
#define __ROMKONAMI4_HH__

#include "Rom8kBBlocks.hh"


namespace openmsx {

class RomKonami4 : public Rom8kBBlocks
{
	public:
		RomKonami4(Device* config, const EmuTime &time, Rom *rom);
		virtual ~RomKonami4();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
