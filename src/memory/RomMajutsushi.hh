// $Id$

#ifndef __ROMMAJUTSUSHI_HH__
#define __ROMMAJUTSUSHI_HH__

#include "Rom8kBBlocks.hh"


class RomMajutsushi : public Rom8kBBlocks
{
	public:
		RomMajutsushi(Device* config, const EmuTime &time);
		virtual ~RomMajutsushi();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;

	private:
		class DACSound8U* dac;
};

#endif
