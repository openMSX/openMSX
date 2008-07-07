// $Id$

#ifndef ROMGENERIC16KB_HH
#define ROMGENERIC16KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomGeneric16kB : public Rom16kBBlocks
{
public:
	RomGeneric16kB(MSXMotherBoard& motherBoard, const XMLElement& config,
	               std::auto_ptr<Rom> rom);

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

REGISTER_MSXDEVICE(RomGeneric16kB, "RomGeneric16kB");

} // namespace openmsx

#endif
