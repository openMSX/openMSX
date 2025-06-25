#ifndef ROMZEMINA25IN1_HH
#define ROMZEMINA25IN1_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomZemina25in1 final : public Rom8kBBlocks
{
public:
	RomZemina25in1(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
