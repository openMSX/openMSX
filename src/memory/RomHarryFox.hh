// $Id$

#ifndef __ROMHARRYFOX_HH__
#define __ROMHARRYFOX_HH__

#include "Rom16kBBlocks.hh"

namespace openmsx {

class RomHarryFox : public Rom16kBBlocks
{
public:
	RomHarryFox(const XMLElement& config, const EmuTime& time,
	            std::auto_ptr<Rom> rom);
	virtual ~RomHarryFox();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
