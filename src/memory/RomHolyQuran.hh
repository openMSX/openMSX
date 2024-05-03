#ifndef ROMHOLYQURAN_HH
#define ROMHOLYQURAN_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomHolyQuran final : public Rom8kBBlocks
{
public:
	RomHolyQuran(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;
};

} // namespace openmsx

#endif
