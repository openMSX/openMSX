#ifndef ROMHARRYFOX_HH
#define ROMHARRYFOX_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomHarryFox : public Rom16kBBlocks
{
public:
	RomHarryFox(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
