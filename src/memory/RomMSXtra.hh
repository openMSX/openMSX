#ifndef ROMMSXTRA_HH
#define ROMMSXTRA_HH

#include "MSXRom.hh"
#include "Ram.hh"

namespace openmsx {

class RomMSXtra final : public MSXRom
{
public:
	RomMSXtra(const DeviceConfig& config, Rom&& rom);

	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Ram ram;
};

} // namespace openmsx

#endif
