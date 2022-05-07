#ifndef ROMDOOLY_HH
#define ROMDOOLY_HH

#include "MSXRom.hh"
#include "RomBlockDebuggable.hh"

namespace openmsx {

class RomDooly final : public MSXRom
{
public:
	RomDooly(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	RomBlockDebuggable romBlockDebug;
	byte conversion;
};

} // namspace openmsx

#endif
