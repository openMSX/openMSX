// $Id$

#ifndef __ROMGENERIC16KB_HH__
#define __ROMGENERIC16KB_HH__

#include "Rom16kBBlocks.hh"

namespace openmsx {

class RomGeneric16kB : public Rom16kBBlocks
{
public:
	RomGeneric16kB(const XMLElement& config, const EmuTime& time,
	               std::auto_ptr<Rom> rom);
	virtual ~RomGeneric16kB();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
