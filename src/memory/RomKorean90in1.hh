// $Id$

#ifndef ROMKOREAN90IN1_HH
#define ROMKOREAN90IN1_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomKorean90in1 : public Rom8kBBlocks
{
public:
	RomKorean90in1(const XMLElement& config, const EmuTime& time,
	               std::auto_ptr<Rom> rom);
	virtual ~RomKorean90in1();

	virtual void reset(const EmuTime& time);
	void writeIO(byte port, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
