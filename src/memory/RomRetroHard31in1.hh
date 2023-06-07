#ifndef ROMRETROHARD31IN1_HH
#define ROMRETROHARD31IN1_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomRetroHard31in1 final : public Rom16kBBlocks
{
public:
	RomRetroHard31in1(const DeviceConfig& config, Rom&& rom);
	~RomRetroHard31in1() override;

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;
};

} // namespace openmsx

#endif
