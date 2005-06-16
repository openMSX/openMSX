// $Id$

#ifndef ROMKOREAN80IN1_HH
#define ROMKOREAN80IN1_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomKorean80in1 : public Rom8kBBlocks
{
public:
	RomKorean80in1(MSXMotherBoard& motherBoard, const XMLElement& config,
	               const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomKorean80in1();

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
