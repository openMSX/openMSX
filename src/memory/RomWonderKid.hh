#ifndef ROMWONDERKID_HH
#define ROMWONDERKID_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomWonderKid final : public Rom16kBBlocks
{
public:
	RomWonderKid(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
