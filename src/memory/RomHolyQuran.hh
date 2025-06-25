#ifndef ROMHOLYQURAN_HH
#define ROMHOLYQURAN_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomHolyQuran final : public Rom8kBBlocks
{
public:
	RomHolyQuran(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
