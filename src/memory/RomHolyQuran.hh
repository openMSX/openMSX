// $Id$

#ifndef __ROMHOLYQURAN_HH__
#define __ROMHOLYQURAN_HH__

#include "Rom8kBBlocks.hh"


class RomHolyQuran : public Rom8kBBlocks
{
	public:
		RomHolyQuran(Device* config, const EmuTime &time, Rom *rom);
		virtual ~RomHolyQuran();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;
};

#endif
