// $Id$

#ifndef __ROMASCII8_8_HH__
#define __ROMASCII8_8_HH__

#include "Rom8kBBlocks.hh"
#include "SRAM.hh"


namespace openmsx {

class RomAscii8_8 : public Rom8kBBlocks
{
	public:
		RomAscii8_8(Device* config, const EmuTime &time, Rom *rom);
		virtual ~RomAscii8_8();
		
		virtual void reset(const EmuTime &time);
		virtual void writeMem(word address, byte value,
		                      const EmuTime &time);
		virtual byte* getWriteCacheLine(word address) const;

	private:
		SRAM sram;
		byte sramEnabled;
};

} // namespace openmsx

#endif
