// $Id$

#ifndef __ROMMAJUTSUSHI_HH__
#define __ROMMAJUTSUSHI_HH__

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomMajutsushi : public Rom8kBBlocks
{
public:
	RomMajutsushi(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~RomMajutsushi();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	class DACSound8U* dac;
};

} // namespace openmsx

#endif
