// $Id$

#ifndef ROMRTYPE_HH
#define ROMRTYPE_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomRType : public Rom16kBBlocks
{
public:
	RomRType(MSXMotherBoard& motherBoard, const XMLElement& config,
	         std::auto_ptr<Rom> rom);

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

REGISTER_MSXDEVICE(RomRType, "RomRType");

} // namespace openmsx

#endif
