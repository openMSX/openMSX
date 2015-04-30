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
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	SamplePlayer samplePlayer;
};

} // namespace openmsx

#endif
