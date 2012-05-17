// $Id$

#ifndef ROMHALNOTE_HH
#define ROMHALNOTE_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomHalnote : public Rom8kBBlocks
{
public:
	RomHalnote(MSXMotherBoard& motherBoard, const DeviceConfig& config,
	           std::auto_ptr<Rom> rom);
	virtual ~RomHalnote();

	virtual void reset(EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte subBanks[2];
	bool sramEnabled;
	bool subMapperEnabled;
};

} // namespace openmsx

#endif
