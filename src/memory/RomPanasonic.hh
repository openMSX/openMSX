// $Id$

#ifndef __ROMPANASONIC_HH__
#define __ROMPANASONIC_HH__

#include "Rom8kBBlocks.hh"


class RomPanasonic : public Rom8kBBlocks
{
	public:
		RomPanasonic(Device* config, const EmuTime &time);
		virtual ~RomPanasonic();
		
		virtual void reset(const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual const byte* getReadCacheLine(word address) const;
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
	
	private:
		byte control;
		int bankSelect[8];
		
};

#endif
