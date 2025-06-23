#ifndef ROMHOLYQURAN2_HH
#define ROMHOLYQURAN2_HH

#include "MSXRom.hh"
#include "RomBlockDebuggable.hh"
#include <array>

namespace openmsx {

class RomHolyQuran2 final : public MSXRom
{
public:
	RomHolyQuran2(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime::param time) const override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct Blocks final : RomBlockDebuggableBase {
		explicit Blocks(const RomHolyQuran2& device);
		[[nodiscard]] unsigned readExt(unsigned address) override;
	} romBlocks;

	std::array<const byte*, 4> bank;
	bool decrypt;
};

} // namespace openmsx

#endif
