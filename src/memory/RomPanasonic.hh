#ifndef ROMPANASONIC_HH
#define ROMPANASONIC_HH

#include "RomBlocks.hh"

namespace openmsx {

class PanasonicMemory;

class RomPanasonic final : public Rom8kBBlocks
{
public:
	RomPanasonic(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void changeBank(byte region, int bank);

	PanasonicMemory& panasonicMem;
	int maxSRAMBank;

	int bankSelect[8];
	byte control;
};

} // namespace openmsx

#endif
