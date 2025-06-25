#ifndef ROMRETROHARD31IN1_HH
#define ROMRETROHARD31IN1_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomRetroHard31in1 final : public Rom16kBBlocks
{
public:
	RomRetroHard31in1(const DeviceConfig& config, Rom&& rom);
	~RomRetroHard31in1() override;

	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
