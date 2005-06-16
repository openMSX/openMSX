// $Id$

#ifndef ROMKONAMI4_HH
#define ROMKONAMI4_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomKonami4 : public Rom8kBBlocks
{
public:
	RomKonami4(MSXMotherBoard& motherBoard, const XMLElement& config,
	           const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomKonami4();

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
