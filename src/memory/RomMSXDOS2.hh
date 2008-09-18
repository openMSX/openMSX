// $Id$

#ifndef ROMMSXDOS2_HH
#define ROMMSXDOS2_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomMSXDOS2 : public Rom16kBBlocks
{
public:
	RomMSXDOS2(MSXMotherBoard& motherBoard, const XMLElement& config,
	           std::auto_ptr<Rom> rom);

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	const byte range;
};

} // namespace openmsx

#endif
