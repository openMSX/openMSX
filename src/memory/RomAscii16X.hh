#ifndef ROMASCII16X_HH
#define ROMASCII16X_HH

#include "MSXRom.hh"
#include "AmdFlash.hh"
#include "RomBlockDebuggable.hh"
#include <array>

namespace openmsx {

class RomAscii16X final : public MSXRom
{
public:
	RomAscii16X(DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] unsigned getFlashAddr(uint16_t addr) const;

	struct Debuggable final : RomBlockDebuggableBase {
		explicit Debuggable(const RomAscii16X& device)
			: RomBlockDebuggableBase(device) {}
		[[nodiscard]] unsigned readExt(unsigned address) override;
	} debuggable;

	AmdFlash flash;

	std::array<uint16_t, 2> bankRegs = {0, 0};
};

} // namespace openmsx

#endif
