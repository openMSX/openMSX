// $Id$

#ifndef ROMPANASONIC_HH
#define ROMPANASONIC_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class SRAM;

class RomPanasonic : public Rom8kBBlocks
{
public:
	RomPanasonic(const XMLElement& config, const EmuTime& time,
	             std::auto_ptr<Rom> rom);
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
	std::auto_ptr<SRAM> sram;
	int maxSRAMBank;
};

} // namespace openmsx

#endif
