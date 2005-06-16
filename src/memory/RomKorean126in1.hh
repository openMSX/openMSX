// $Id$

#ifndef ROMKOREAN126IN1_HH
#define ROMKOREAN126IN1_HH

#include "Rom16kBBlocks.hh"

namespace openmsx {

class RomKorean126in1 : public Rom16kBBlocks
{
public:
	RomKorean126in1(MSXMotherBoard& motherBoard, const XMLElement& config,
	                const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomKorean126in1();

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
