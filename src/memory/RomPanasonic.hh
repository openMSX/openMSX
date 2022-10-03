#ifndef ROMPANASONIC_HH
#define ROMPANASONIC_HH

#include "RomBlocks.hh"
#include <array>

namespace openmsx {

class PanasonicMemory;

class RomPanasonic final : public Rom8kBBlocks
{
public:
	RomPanasonic(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void changeBank(byte region, int bank);

private:
	PanasonicMemory& panasonicMem;
	int maxSRAMBank;
	std::array<int, 8> bankSelect;
	byte control;
};

} // namespace openmsx

#endif
