#ifndef ROMMATRACOMPILATION_HH
#define ROMMATRACOMPILATION_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomMatraCompilation final : public Rom8kBBlocks
{
public:
	RomMatraCompilation(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte blockOffset;
};

} // namespace openmsx

#endif
