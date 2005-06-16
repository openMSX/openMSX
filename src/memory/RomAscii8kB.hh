// $Id$

#ifndef ROMASCII8KB_HH
#define ROMASCII8KB_HH

#include "Rom8kBBlocks.hh"

namespace openmsx {

class RomAscii8kB : public Rom8kBBlocks
{
public:
	RomAscii8kB(MSXMotherBoard& motherBoard, const XMLElement& config,
	            const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomAscii8kB();

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
