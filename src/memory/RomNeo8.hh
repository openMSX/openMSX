#ifndef ROMNEO8_HH
#define ROMNEO8_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomNeo8 final : public Rom8kBBlocks
{
public:
	RomNeo8(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::array<uint16_t, 6> blockReg;
};

} // namespace openmsx

#endif
