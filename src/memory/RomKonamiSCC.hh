// $Id$

#ifndef ROMKONAMISCC_HH
#define ROMKONAMISCC_HH

#include "RomBlocks.hh"

namespace openmsx {

class SCC;

class RomKonamiSCC : public Rom8kBBlocks
{
public:
	RomKonamiSCC(MSXMotherBoard& motherBoard, const XMLElement& config,
	           std::auto_ptr<Rom> rom);
	virtual ~RomKonamiSCC();

	virtual void reset(const EmuTime& time);
	virtual byte peekMem(word address, const EmuTime& time) const;
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::auto_ptr<SCC> scc;
	bool sccEnabled;
};

REGISTER_MSXDEVICE(RomKonamiSCC, "RomKonamiSCC");

} // namespace openmsx

#endif
