// $Id$

#ifndef __ROMKONAMI4_HH__
#define __ROMKONAMI4_HH__

#include "Rom8kBBlocks.hh"


class RomKonami4 : public Rom8kBBlocks
{
	public:
		RomKonami4(Device* config, const EmuTime &time);
		virtual ~RomKonami4();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
};

#endif
