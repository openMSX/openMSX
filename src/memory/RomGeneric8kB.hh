// $Id$

#ifndef ROMGENERIC8KB_HH
#define ROMGENERIC8KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomGeneric8kB : public Rom8kBBlocks
{
public:
	RomGeneric8kB(MSXMotherBoard& motherBoard, const XMLElement& config,
	              std::auto_ptr<Rom> rom);

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

REGISTER_MSXDEVICE(RomGeneric8kB, "RomGeneric8kB");

} // namespace openmsx

#endif
