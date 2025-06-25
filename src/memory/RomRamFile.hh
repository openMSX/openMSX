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

	void reset(EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::unique_ptr<SRAM> sram;
	MSXCPU& cpu;
	byte shiftValue;
};

} // namespace openmsx

#endif
