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

	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void changeBank(unsigned region, unsigned bank);

private:
	PanasonicMemory& panasonicMem;
	unsigned maxSRAMBank;
	std::array<unsigned, 8> bankSelect;
	byte control;
};

} // namespace openmsx

#endif
