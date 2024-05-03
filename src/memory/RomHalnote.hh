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
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::array<byte, 2> subBanks;
	bool sramEnabled;
	bool subMapperEnabled;
};

} // namespace openmsx

#endif
