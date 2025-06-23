#ifndef ROMZEMINA126IN1_HH
#define ROMZEMINA126IN1_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomZemina126in1 final : public Rom16kBBlocks
{
public:
	RomZemina126in1(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
