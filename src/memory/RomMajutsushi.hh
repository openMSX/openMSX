// $Id$

#ifndef ROMMAJUTSUSHI_HH
#define ROMMAJUTSUSHI_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class DACSound8U;

class RomMajutsushi : public Rom8kBBlocks
{
public:
	RomMajutsushi(const XMLElement& config, const EmuTime& time,
	              std::auto_ptr<Rom> rom);
	virtual ~RomMajutsushi();
	
	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	std::auto_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif
