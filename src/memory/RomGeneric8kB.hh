// $Id$

#ifndef __ROMGENERIC8KB_HH__
#define __ROMGENERIC8KB_HH__

#include "Rom8kBBlocks.hh"


namespace openmsx {

class RomGeneric8kB : public Rom8kBBlocks
{
	public:
		RomGeneric8kB(Device* config, const EmuTime &time, Rom *rom);
		virtual ~RomGeneric8kB();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
