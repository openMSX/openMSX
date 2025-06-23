#ifndef KONAMIULTIMATECOLLECTION_HH
#define KONAMIULTIMATECOLLECTION_HH

#include "AmdFlash.hh"
#include "DACSound8U.hh"
#include "MSXRom.hh"
#include "RomBlockDebuggable.hh"
#include "SCC.hh"

#include <array>

namespace openmsx {

class KonamiUltimateCollection final : public MSXRom
{
public:
	KonamiUltimateCollection(DeviceConfig& config, Rom&& rom);

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] bool isSCCAccess(uint16_t addr) const;
	[[nodiscard]] unsigned getFlashAddr(unsigned addr) const;

	[[nodiscard]] bool isKonamiSCCmode()         const { return (mapperReg & 0x20) == 0; }
	[[nodiscard]] bool isFlashRomWriteEnabled()  const { return (mapperReg & 0x10) != 0; }
	[[nodiscard]] bool isBank0Disabled()         const { return (mapperReg & 0x08) != 0; }
	[[nodiscard]] bool isMapperRegisterEnabled() const { return (mapperReg & 0x04) == 0; }
	[[nodiscard]] bool areBankRegsEnabled()      const { return (mapperReg & 0x02) == 0; }

private:
	struct Blocks final : RomBlockDebuggableBase {
		explicit Blocks(const KonamiUltimateCollection& device)
			: RomBlockDebuggableBase(device) {}
		[[nodiscard]] unsigned readExt(unsigned address) override;
	} romBlockDebug;

	AmdFlash flash;
	SCC scc;
	DACSound8U dac;

	byte mapperReg;
	byte offsetReg;
	byte sccMode;
	std::array<byte, 4> bankRegs;
};

} // namespace openmsx

#endif
