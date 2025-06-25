#ifndef ROMGENERIC16KB_HH
#define ROMGENERIC16KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomGeneric16kB final : public Rom16kBBlocks
{
public:
	RomGeneric16kB(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
