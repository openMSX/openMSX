// $Id$

#ifndef ROMCROSSBLAIM_HH
#define ROMCROSSBLAIM_HH

#include "Rom16kBBlocks.hh"

namespace openmsx {

class RomCrossBlaim : public Rom16kBBlocks
{
public:
	RomCrossBlaim(MSXMotherBoard& motherBoard, const XMLElement& config,
	              const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomCrossBlaim();

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
