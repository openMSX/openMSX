// $Id$

#ifndef __ROMRTYPE_HH__
#define __ROMRTYPE_HH__

#include "Rom16kBBlocks.hh"


class RomRType : public Rom16kBBlocks
{
	public:
		RomRType(Device* config, const EmuTime &time);
		virtual ~RomRType();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;
};

#endif
