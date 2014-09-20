#ifndef ROMPLAYBALL_HH
#define ROMPLAYBALL_HH

#include "RomBlocks.hh"

namespace openmsx {

class SamplePlayer;

class RomPlayBall final : public Rom16kBBlocks
{
public:
	RomPlayBall(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~RomPlayBall();

	void reset(EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<SamplePlayer> samplePlayer;
};

} // namespace openmsx

#endif
