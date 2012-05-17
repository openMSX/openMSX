// $Id$

#ifndef ROMRTYPE_HH
#define ROMRTYPE_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomRType : public Rom16kBBlocks
{
public:
	RomRType(const DeviceConfig& config, std::auto_ptr<Rom> rom);

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
