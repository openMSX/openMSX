#ifndef ROMPLAYBALL_HH
#define ROMPLAYBALL_HH

#include "RomBlocks.hh"
#include "SamplePlayer.hh"

namespace openmsx {

class RomPlayBall final : public Rom16kBBlocks
{
public:
	RomPlayBall(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	SamplePlayer samplePlayer;
};

} // namespace openmsx

#endif
