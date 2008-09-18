// $Id$

#ifndef ROMZEMINA126IN1_HH
#define ROMZEMINA126IN1_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomZemina126in1 : public Rom16kBBlocks
{
public:
	RomZemina126in1(MSXMotherBoard& motherBoard, const XMLElement& config,
	                std::auto_ptr<Rom> rom);

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
