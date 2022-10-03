#ifndef ROMNATIONAL_HH
#define ROMNATIONAL_HH

#include "RomBlocks.hh"
#include <array>

namespace openmsx {

class RomNational final : public Rom16kBBlocks
{
public:
	RomNational(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	int sramAddr;
	byte control;
	std::array<byte, 4> bankSelect;
};

} // namespace openmsx

#endif
