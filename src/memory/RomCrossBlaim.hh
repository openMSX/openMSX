// $Id$

#ifndef ROMCROSSBLAIM_HH
#define ROMCROSSBLAIM_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomCrossBlaim : public Rom16kBBlocks
{
public:
	RomCrossBlaim(MSXMotherBoard& motherBoard, const XMLElement& config,
	              std::auto_ptr<Rom> rom);

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

REGISTER_MSXDEVICE(RomCrossBlaim, "RomCrossBlaim");

} // namespace openmsx

#endif
