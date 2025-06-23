#ifndef ROMGAMEMASTER2_HH
#define ROMGAMEMASTER2_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomGameMaster2 final : public Rom4kBBlocks
{
public:
	RomGameMaster2(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	uint16_t sramOffset;
	bool sramEnabled;
};

} // namespace openmsx

#endif
