// $Id$

#ifndef __ROMKOREAN90IN1_HH__
#define __ROMKOREAN90IN1_HH__

#include "Rom8kBBlocks.hh"
#include "MSXIODevice.hh"

namespace openmsx {

class RomKorean90in1 : public Rom8kBBlocks, public MSXIODevice
{
public:
	RomKorean90in1(const XMLElement& config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~RomKorean90in1();
	
	virtual void reset(const EmuTime& time);
	void writeIO(byte port, byte value, const EmuTime & time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
