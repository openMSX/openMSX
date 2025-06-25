#ifndef ROMZEMINA90IN1_HH
#define ROMZEMINA90IN1_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomZemina90in1 final : public Rom8kBBlocks
{
public:
	RomZemina90in1(const DeviceConfig& config, Rom&& rom);
	~RomZemina90in1() override;

	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
