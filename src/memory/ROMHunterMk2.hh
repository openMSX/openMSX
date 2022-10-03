#ifndef ROMHUNTERMK2_HH
#define ROMHUNTERMK2_HH

#include "MSXRom.hh"
#include "Ram.hh"
#include <array>

namespace openmsx {

class ROMHunterMk2 final : public MSXRom
{
public:
	ROMHunterMk2(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] unsigned getRamAddr(unsigned addr) const;

private:
	Ram ram;
	byte configReg;
	std::array<byte, 4> bankRegs;
};

} // namespace openmsx

#endif
