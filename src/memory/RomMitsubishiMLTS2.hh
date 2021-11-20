#ifndef ROMMITSUBISHIMLTS2_HH
#define ROMMITSUBISHIMLTS2_HH

#include "RomBlocks.hh"
#include "Ram.hh"

// PLEASE NOTE!
//
// This mapper is work in progress. It's just a guess based on some reverse
// engineering of the ROM by BiFi. It even contains some debug prints :)

namespace openmsx {

class RomMitsubishiMLTS2 final : public Rom8kBBlocks
{
public:
	RomMitsubishiMLTS2(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Ram ram;
};

} // namespace openmsx

#endif
