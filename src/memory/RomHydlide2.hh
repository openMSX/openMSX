// $Id$

#ifndef __ROMHYDLIDE2_HH__
#define __ROMHYDLIDE2_HH__

#include "RomAscii16kB.hh"
#include "SRAM.hh"

namespace openmsx {

class RomHydlide2 : public RomAscii16kB
{
public:
	RomHydlide2(Config* config, const EmuTime& time, Rom* rom);
	virtual ~RomHydlide2();
	
	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	SRAM sram;
	byte sramEnabled;
};

} // namespace openmsx

#endif
