#ifndef ROMMATRAINK_HH
#define ROMMATRAINK_HH

#include "MSXRom.hh"
#include "AmdFlash.hh"

namespace openmsx {

class RomMatraInk final : public MSXRom
{
public:
	RomMatraInk(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	AmdFlash flash;
};

} // namespace openmsx

#endif
