// $Id$

#ifndef __ROMCROSSBLAIM_HH__
#define __ROMCROSSBLAIM_HH__

#include "Rom16kBBlocks.hh"


class RomCrossBlaim : public Rom16kBBlocks
{
	public:
		RomCrossBlaim(Device* config, const EmuTime &time);
		virtual ~RomCrossBlaim();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
};

#endif
