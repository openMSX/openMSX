// $Id$

#ifndef __ROMRTYPE_HH__
#define __ROMRTYPE_HH__

#include "Rom16kBBlocks.hh"

namespace openmsx {

class RomRType : public Rom16kBBlocks
{
public:
	RomRType(const XMLElement& config, const EmuTime& time,
	         std::auto_ptr<Rom> rom);
	virtual ~RomRType();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
