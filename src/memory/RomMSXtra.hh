#ifndef ROMMSXTRA_HH
#define ROMMSXTRA_HH

#include "MSXRom.hh"
#include "Ram.hh"

namespace openmsx {

class RomMSXtra final : public MSXRom
{
public:
	RomMSXtra(const DeviceConfig& config, Rom&& rom);

	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Ram ram;
};

} // namespace openmsx

#endif
