// $Id$

#ifndef __ROMHARRYFOX_HH__
#define __ROMHARRYFOX_HH__

#include "Rom16kBBlocks.hh"


class RomHarryFox : public Rom16kBBlocks
{
	public:
		RomHarryFox(Device* config, const EmuTime &time);
		virtual ~RomHarryFox();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;
};

#endif
