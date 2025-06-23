#ifndef ROMMATRAINK_HH
#define ROMMATRAINK_HH

#include "MSXRom.hh"
#include "AmdFlash.hh"

namespace openmsx {

class RomMatraInk final : public MSXRom
{
public:
	RomMatraInk(DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	AmdFlash flash;
};

} // namespace openmsx

#endif
