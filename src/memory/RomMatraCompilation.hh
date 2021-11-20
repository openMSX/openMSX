#ifndef ROMMATRACOMPILATION_HH
#define ROMMATRACOMPILATION_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomMatraCompilation final : public Rom8kBBlocks
{
public:
	RomMatraCompilation(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte blockOffset;
};

} // namespace openmsx

#endif
