// $Id$

#ifndef __ROMPANASONIC_HH__
#define __ROMPANASONIC_HH__

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomPanasonic : public Rom8kBBlocks
{
public:
	RomPanasonic(Config* config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~RomPanasonic();
	
	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	void changeBank(byte region, int bank);
	
	byte control;
	int bankSelect[8];
	class SRAM* sram;
	int maxSRAMBank;
};

} // namespace openmsx

#endif
