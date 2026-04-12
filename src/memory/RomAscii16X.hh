#ifndef ROMASCII16X_HH
#define ROMASCII16X_HH

#include "AmdFlash.hh"
#include "MSXRom.hh"
#include "RomBlockDebuggable.hh"

#include <array>

namespace openmsx {

class RomAscii16X final : public MSXRom
{
public:
	RomAscii16X(DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] unsigned getFlashAddr(uint16_t addr) const;

	struct Debuggable final : RomBlockDebuggableBase {
		explicit Debuggable(const RomAscii16X& device)
			: RomBlockDebuggableBase(device) {}
		explicit Debuggable(const RomAscii16X& device, std::string name_)
			: RomBlockDebuggableBase(device, name_) {}
		[[nodiscard]] unsigned readExt(unsigned address) override;
	} debuggable, debuggableExt;

	AmdFlash flash;

	std::array<uint8_t, 2> mappedLow = {0, 0};
	std::array<uint8_t, 2> mappedHigh = {0, 0};
};

} // namespace openmsx

#endif
