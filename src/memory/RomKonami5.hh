// $Id$

#ifndef ROMKONAMI5_HH
#define ROMKONAMI5_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class SCC;

class RomKonami5 : public Rom8kBBlocks
{
public:
	RomKonami5(const XMLElement& config, const EmuTime& time,
	           std::auto_ptr<Rom> rom);
	virtual ~RomKonami5();

	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	std::auto_ptr<SCC> scc;
	bool sccEnabled;
};

} // namespace openmsx

#endif
