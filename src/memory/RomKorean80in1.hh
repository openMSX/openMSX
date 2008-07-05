// $Id$

#ifndef ROMKOREAN80IN1_HH
#define ROMKOREAN80IN1_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomKorean80in1 : public Rom8kBBlocks
{
public:
	RomKorean80in1(MSXMotherBoard& motherBoard, const XMLElement& config,
	               std::auto_ptr<Rom> rom);

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
