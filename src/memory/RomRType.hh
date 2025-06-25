#ifndef ROMRTYPE_HH
#define ROMRTYPE_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomRType final : public Rom16kBBlocks
{
public:
	RomRType(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
