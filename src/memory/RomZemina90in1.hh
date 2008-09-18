// $Id$

#ifndef ROMZEMINA90IN1_HH
#define ROMZEMINA90IN1_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomZemina90in1 : public Rom8kBBlocks
{
public:
	RomZemina90in1(MSXMotherBoard& motherBoard, const XMLElement& config,
	               std::auto_ptr<Rom> rom);
	virtual ~RomZemina90in1();

	virtual void reset(const EmuTime& time);
	void writeIO(word port, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
