// $Id$

#ifndef __ROMASCII8_8_HH__
#define __ROMASCII8_8_HH__

#include "Rom8kBBlocks.hh"
#include "SRAM.hh"

namespace openmsx {

class RomAscii8_8 : public Rom8kBBlocks
{
public:
	enum SubType { ASCII8_8, KOEI_8, KOEI_32, WIZARDRY };
	RomAscii8_8(const XMLElement& config, const EmuTime& time,
	            std::auto_ptr<Rom> rom, SubType subType);
	virtual ~RomAscii8_8();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	SRAM sram;
	byte sramEnabled;
	byte sramBlock[8];
	byte sramEnableBit;
	byte sramPages;
};

} // namespace openmsx

#endif
