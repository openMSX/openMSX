// $Id$

#ifndef __ROMGAMEMASTER2_HH__
#define __ROMGAMEMASTER2_HH__

#include "Rom4kBBlocks.hh"
#include "SRAM.hh"


class RomGameMaster2 : public Rom4kBBlocks
{
	public:
		RomGameMaster2(Device* config, const EmuTime &time);
		virtual ~RomGameMaster2();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;
	
	private:
		SRAM sram;
		bool sramEnabled;
};

#endif
