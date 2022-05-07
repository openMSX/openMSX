#ifndef ROMHOLYQURAN2_HH
#define ROMHOLYQURAN2_HH

#include "MSXRom.hh"
#include "RomBlockDebuggable.hh"

namespace openmsx {

class RomHolyQuran2 final : public MSXRom
{
public:
	RomHolyQuran2(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	struct Blocks final : RomBlockDebuggableBase {
		explicit Blocks(RomHolyQuran2& device);
		[[nodiscard]] byte read(unsigned address) override;
	} romBlocks;

	const byte* bank[4];
	bool decrypt;
};

} // namespace openmsx

#endif
