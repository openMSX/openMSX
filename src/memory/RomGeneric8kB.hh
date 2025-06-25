#ifndef ROMGENERIC8KB_HH
#define ROMGENERIC8KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomGeneric8kB final : public Rom8kBBlocks
{
public:
	RomGeneric8kB(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
