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
	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::unique_ptr<SRAM> sram;
	MSXCPU& cpu;
	byte shiftValue;
};

} // namespace openmsx

#endif
