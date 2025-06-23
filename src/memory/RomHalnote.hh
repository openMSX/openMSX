#ifndef ROMHALNOTE_HH
#define ROMHALNOTE_HH

#include "RomBlocks.hh"
#include <array>

namespace openmsx {

class RomHalnote final : public Rom8kBBlocks
{
public:
	RomHalnote(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::array<byte, 2> subBanks;
	bool sramEnabled;
	bool subMapperEnabled;
};

} // namespace openmsx

#endif
