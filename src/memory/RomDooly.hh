#ifndef ROMDOOLY_HH
#define ROMDOOLY_HH

#include "MSXRom.hh"
#include "RomBlockDebuggable.hh"

namespace openmsx {

class RomDooly final : public MSXRom
{
public:
	RomDooly(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	RomBlockDebuggable romBlockDebug;
	byte conversion = 0;
};

} // namespace openmsx

#endif
