// $Id$

#ifndef ROMHOLYQURAN_HH
#define ROMHOLYQURAN_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomHolyQuran : public Rom8kBBlocks
{
public:
	RomHolyQuran(MSXMotherBoard& motherBoard, const XMLElement& config,
	             const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomHolyQuran();

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
