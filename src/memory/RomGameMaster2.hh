// $Id$

#ifndef __ROMGAMEMASTER2_HH__
#define __ROMGAMEMASTER2_HH__

#include "Rom4kBBlocks.hh"
#include <memory>

namespace openmsx {

class SRAM;

class RomGameMaster2 : public Rom4kBBlocks
{
public:
	RomGameMaster2(const XMLElement& config, const EmuTime& time,
	               std::auto_ptr<Rom> rom);
	virtual ~RomGameMaster2();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	const std::auto_ptr<SRAM> sram;
	bool sramEnabled;
};

} // namespace openmsx

#endif
