// $Id$

#ifndef __ROMPANASONIC_HH__
#define __ROMPANASONIC_HH__

#include <memory>
#include "Rom8kBBlocks.hh"

using std::auto_ptr;

namespace openmsx {

class SRAM;

class RomPanasonic : public Rom8kBBlocks
{
public:
	RomPanasonic(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom);
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
	auto_ptr<SRAM> sram;
	int maxSRAMBank;
};

} // namespace openmsx

#endif
