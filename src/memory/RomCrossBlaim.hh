#ifndef ROMCROSSBLAIM_HH
#define ROMCROSSBLAIM_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomCrossBlaim : public Rom16kBBlocks
{
public:
	RomCrossBlaim(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
