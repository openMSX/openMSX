#ifndef KONAMIULTIMATECOLLECTION_HH
#define kONAMIULTIMATECOLLECTION_HH

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
	enum SCCEnable { EN_NONE, EN_SCC, EN_SCCPLUS };
	SCCEnable getSCCEnable() const;

	/**
	 * Writes to flash only if it was not write protected.
	 */
	void writeToFlash(unsigned addr, byte value);
	AmdFlash flash;
        unsigned getFlashAddr(unsigned addr) const;

	SCC scc;
	DACSound8U dac;

	byte mapperReg;
	bool isKonamiSCCmapperConfigured()         const { return (mapperReg & 0x20) == 0; }
	bool isFlashRomWriteEnabled()              const { return (mapperReg & 0x10) != 0; }
	bool isWritingKonamiBankRegisterDisabled() const { return (mapperReg & 0x08) != 0; }
	bool isMapperRegisterDisabled()            const { return (mapperReg & 0x04) != 0; }
	bool areBankRegsAndOffsetRegsDisabled()    const { return (mapperReg & 0x02) != 0; }
	bool areKonamiMapperLimitsEnabled()        const { return (mapperReg & 0x01) != 0; }
	unsigned offsetReg;

	byte bankRegs[4];
	byte sccMode;
	byte sccBanks[4];
};

} // namespace openmsx

#endif
