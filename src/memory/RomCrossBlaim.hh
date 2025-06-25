#ifndef ROMCROSSBLAIM_HH
#define ROMCROSSBLAIM_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomCrossBlaim final : public Rom16kBBlocks
{
public:
	RomCrossBlaim(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
