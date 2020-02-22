#ifndef KONAMIULTIMATECOLLECTION_HH
#define KONAMIULTIMATECOLLECTION_HH

#include "MSXRom.hh"
#include "AmdFlash.hh"
#include "SCC.hh"
#include "DACSound8U.hh"

namespace openmsx {

class KonamiUltimateCollection final : public MSXRom
{
public:
	KonamiUltimateCollection(const DeviceConfig& config, Rom&& rom);

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	bool isSCCAccess(word addr) const;
	unsigned getFlashAddr(unsigned addr) const;

	bool isKonamiSCCmode()         const { return (mapperReg & 0x20) == 0; }
	bool isFlashRomWriteEnabled()  const { return (mapperReg & 0x10) != 0; }
	bool isBank0Disabled()         const { return (mapperReg & 0x08) != 0; }
	bool isMapperRegisterEnabled() const { return (mapperReg & 0x04) == 0; }
	bool areBankRegsEnabled()      const { return (mapperReg & 0x02) == 0; }

private:
	AmdFlash flash;
	SCC scc;
	DACSound8U dac;

	byte mapperReg;
	byte offsetReg;
	byte sccMode;
	byte bankRegs[4];
};

} // namespace openmsx

#endif
