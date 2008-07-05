// $Id$

#ifndef ROMZEMINA80IN1_HH
#define ROMZEMINA80IN1_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomZemina80in1 : public Rom8kBBlocks
{
public:
	RomZemina80in1(MSXMotherBoard& motherBoard, const XMLElement& config,
	               std::auto_ptr<Rom> rom);

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

REGISTER_MSXDEVICE(RomZemina80in1, "RomZemina80in1");

} // namespace openmsx

#endif
