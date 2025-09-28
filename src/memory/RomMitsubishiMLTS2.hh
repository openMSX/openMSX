#ifndef ROMMITSUBISHIMLTS2_HH
#define ROMMITSUBISHIMLTS2_HH

#include "Ram.hh"
#include "RomBlocks.hh"

// PLEASE NOTE!
//
// This mapper is work in progress. It's just a guess based on some reverse
// engineering of the ROM by BiFi. It even contains some debug prints :)

namespace openmsx {

class RomMitsubishiMLTS2 final : public Rom8kBBlocks
{
public:
	RomMitsubishiMLTS2(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Ram ram;
};

} // namespace openmsx

#endif
