// $Id$

#ifndef __ROMASCII8KB_HH__
#define __ROMASCII8KB_HH__

#include "Rom8kBBlocks.hh"


namespace openmsx {

class RomAscii8kB : public Rom8kBBlocks
{
	public:
		RomAscii8kB(Device* config, const EmuTime &time, Rom *rom);
		virtual ~RomAscii8kB();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
