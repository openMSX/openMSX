#ifndef ROMNETTOUYAKYUU_HH
#define ROMNETTOUYAKYUU_HH

#include "RomBlocks.hh"
#include "SamplePlayer.hh"

namespace openmsx {

class RomNettouYakyuu final : public Rom8kBBlocks
{
public:
	RomNettouYakyuu(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	SamplePlayer samplePlayer;

	// remember per region if writes are for the sample player or not
	// there are 4 x 8kB regions in [0x4000-0xBFFF]
	bool redirectToSamplePlayer[4];
};

} // namespace openmsx

#endif
