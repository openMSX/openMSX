#ifndef ROMASCII8KB_HH
#define ROMASCII8KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomAscii8kB : public Rom8kBBlocks
{
public:
	RomAscii8kB(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	virtual ~RomAscii8kB() {}

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
