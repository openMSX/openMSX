#ifndef ROMRAMFILE_HH
#define ROMRAMFILE_HH

#include "MSXRom.hh"
#include "SRAM.hh"

namespace openmsx {

class MSXCPU;

class RomRamFile final : public MSXRom
{
public:
	RomRamFile(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	std::unique_ptr<SRAM> sram;
	MSXCPU& cpu;
	byte shiftValue;
};

} // namespace openmsx

#endif
