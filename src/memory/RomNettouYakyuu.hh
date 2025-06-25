#ifndef ROMNETTOUYAKYUU_HH
#define ROMNETTOUYAKYUU_HH

#include "RomBlocks.hh"
#include "SamplePlayer.hh"
#include <array>

namespace openmsx {

class RomNettouYakyuu final : public Rom8kBBlocks
{
public:
	RomNettouYakyuu(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	SamplePlayer samplePlayer;

	// remember per region if writes are for the sample player or not
	// there are 4 x 8kB regions in [0x4000-0xBFFF]
	std::array<bool, 4> redirectToSamplePlayer;
};

} // namespace openmsx

#endif
