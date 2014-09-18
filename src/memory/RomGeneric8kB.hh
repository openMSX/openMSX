#ifndef ROMGENERIC8KB_HH
#define ROMGENERIC8KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomGeneric8kB final : public Rom8kBBlocks
{
public:
	RomGeneric8kB(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
