// $Id$

#ifndef ROMHALNOTE_HH
#define ROMHALNOTE_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class SRAM;

class RomHalnote : public Rom8kBBlocks
{
public:
	RomHalnote(MSXMotherBoard& motherBoard, const XMLElement& config,
	           const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomHalnote();

	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	const std::auto_ptr<SRAM> sram;
	byte subBanks[2];
	bool sramEnabled;
	bool subMapperEnabled;
};

} // namespace openmsx

#endif
