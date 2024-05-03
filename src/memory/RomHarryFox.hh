#ifndef ROMHARRYFOX_HH
#define ROMHARRYFOX_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomHarryFox final : public Rom16kBBlocks
{
public:
	RomHarryFox(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;
};

} // namespace openmsx

#endif
