// $Id$

#ifndef __ROMHALNOTE_HH__
#define __ROMHALNOTE_HH__

#include "Rom8kBBlocks.hh"


class RomHalnote : public Rom8kBBlocks
{
	public:
		RomHalnote(Device* config, const EmuTime &time);
		virtual ~RomHalnote();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
};

#endif
