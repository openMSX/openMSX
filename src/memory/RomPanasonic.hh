// $Id$

#ifndef ROMPANASONIC_HH
#define ROMPANASONIC_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class SRAM;

class RomPanasonic : public Rom8kBBlocks
{
public:
	RomPanasonic(MSXMotherBoard& motherBoard, const XMLElement& config,
	             const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomPanasonic();

	virtual void reset(const EmuTime& time);
	virtual byte peekMem(word address, const EmuTime& time) const;
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	void changeBank(byte region, int bank);

	std::auto_ptr<SRAM> sram;
	int bankSelect[8];
	int maxSRAMBank;
	byte control;
};

} // namespace openmsx

#endif
