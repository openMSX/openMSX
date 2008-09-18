// $Id$

#ifndef ROMKONAMI_HH
#define ROMKONAMI_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomKonami : public Rom8kBBlocks
{
public:
	RomKonami(MSXMotherBoard& motherBoard, const XMLElement& config,
	           std::auto_ptr<Rom> rom);

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

REGISTER_BASE_CLASS(RomKonami, "RomKonami");

} // namespace openmsx

#endif
