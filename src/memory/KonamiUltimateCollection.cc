#include "KonamiUltimateCollection.hh"
#include "serialize.hh"
#include <vector>

/******************************************************************************
 * DOCUMENTATION AS PROVIDED BY MANUEL PAZOS, WHO DEVELOPED THE CARTRIDGE     *
 ******************************************************************************

   Konami Ultimate Collection shares some features of MegaFlashROM SCC+ SD
   It uses the same flashROM, SCC/SCC+, Konami and Konami SCC mappers, etc.

[OFFSET REGISTER (#7FFE)]
7-0: Bank offset

[MAPPER REGISTER (#7FFF)]
7     A21 \
6     A20 / FlashROM address lines to switch 2 MB banks.
5     Mapper mode  :   Select Konami mapper (0=SCC or 1=normal). [1]
4     Write enable
3     Disable #4000-#5FFF mapper in Konami mode, Enable DAC (works like the DAC of Synthesizer or Majutsushi)
2     Disable mapper register
1     Disable mapper
0     Enable 512K mapper limit in SCC mapper or 256K limit in Konami mapper

[1] bit 5 only changes the address range of the mapper (Konami or Konami SCC)
but the SCC is always available.  This feature is inherited from MFC SCC+ subslot
mode because all subslots share the same mapper type. So to make Konami
combinations (i.e.: Konami ROM with Konami SCC ROM) a "common mapper" is needed
and the SCC must be available. So I made a "Konami mapper" with SCC.  In fact,
all Konami and Konami SCC ROMs should work with "Konami" mapper in KUC.

******************************************************************************/

namespace openmsx {

static std::vector<AmdFlash::SectorInfo> getSectorInfo()
{
	std::vector<AmdFlash::SectorInfo> sectorInfo;
	// 8 * 8kB
	sectorInfo.insert(end(sectorInfo), 8, {8 * 1024, false});
	// 127 * 64kB
	sectorInfo.insert(end(sectorInfo), 127, {64 * 1024, false});
	return sectorInfo;
}

KonamiUltimateCollection::KonamiUltimateCollection(
		const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, flash(rom, getSectorInfo(), 0x207E, true, config)
	, scc("KUC SCC", config, getCurrentTime(), SCC::SCC_Compatible)
	, dac("KUC DAC", "Konami Ultimate Collection DAC", config)
{
	powerUp(getCurrentTime());
}

void KonamiUltimateCollection::powerUp(EmuTime::param time)
{
	scc.powerUp(time);
	reset(time);
}

void KonamiUltimateCollection::reset(EmuTime::param time)
{
	mapperReg = 0;
	offsetReg = 0;
	for (int bank = 0; bank < 4; ++bank) {
		bankRegs[bank] = bank;
	}

	sccMode = 0;
	for (int i = 0; i < 4; ++i) {
		sccBanks[i] = i;
	}
	scc.reset(time);
	dac.reset(time);

	invalidateMemCache(0x0000, 0x10000); // flush all to be sure
}

void KonamiUltimateCollection::writeToFlash(unsigned addr, byte value)
{
	if (isFlashRomWriteEnabled()) {
		flash.write(addr, value);
	} else {
		// flash is write protected, this is implemented by not passing
		// writes to flash at all.
	}
}

KonamiUltimateCollection::SCCEnable KonamiUltimateCollection::getSCCEnable() const
{
	if ((sccMode & 0x20) && (sccBanks[3] & 0x80)) {
		return EN_SCCPLUS;
	} else if ((!(sccMode & 0x20)) && ((sccBanks[2] & 0x3F) == 0x3F)) {
		return EN_SCC;
	} else {
		return EN_NONE;
	}
}

unsigned KonamiUltimateCollection::getFlashAddr(unsigned addr) const
{
	unsigned page = ((addr >> 13) - 2);
	unsigned size = 0x2000;

	if (page >= 4) return unsigned(-1); // outside [0x4000, 0xBFFF]

	unsigned bank = bankRegs[page];
	bank += offsetReg;

	unsigned tmp = (bank * size) + (addr & (size - 1));
	return tmp & 0x7FFFFF; // wrap at 8MB
}

byte KonamiUltimateCollection::readMem(word addr, EmuTime::param time)
{
	SCCEnable enable = getSCCEnable();
	if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
	    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
		byte val = scc.readMem(addr & 0xFF, time);
		return val;
	}

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.read(flashAddr)
		: 0xFF; // unmapped read
}

byte KonamiUltimateCollection::peekMem(word addr, EmuTime::param time) const
{
	SCCEnable enable = getSCCEnable();
	if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
	    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
		byte val = scc.peekMem(addr & 0xFF, time);
		return val;
	}

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.peek(flashAddr)
		: 0xFF; // unmapped read
}

const byte* KonamiUltimateCollection::getReadCacheLine(word addr) const
{
	SCCEnable enable = getSCCEnable();
	if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
	    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
		return nullptr;
	}

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.getReadCacheLine(flashAddr)
		: unmappedRead;
}

void KonamiUltimateCollection::writeMem(word addr, byte value, EmuTime::param time)
{
	// address is calculated before writes to other regions take effect
	unsigned flashAddr = getFlashAddr(addr);

	// There are several overlapping functional regions in the address
	// space. A single write can trigger behaviour in multiple regions. In
	// other words there's no priority amongst the regions where a higher
	// priority region blocks the write from the lower priority regions.
	// This only goes for places where the flash is 'seen', so not for the
	// SCC registers

	if (!isMapperRegisterDisabled() && (addr == 0x7FFF)) {
		// write mapper register
		mapperReg = value;
		// write offset register high part (bit 8 and 9)
		offsetReg = (offsetReg & 0xFF) + (((value & 0xC0) >> 6) << 8);

		invalidateMemCache(0x0000, 0x10000); // flush all to be sure
	}

	if (!areBankRegsAndOffsetRegsDisabled() && (addr == 0x7FFE)) {
		// write offset register low part
		offsetReg = (offsetReg & 0x300) | value;
		invalidateMemCache(0x0000, 0x10000);
	}

	// Konami-SCC
	if ((addr & 0xFFFE) == 0xBFFE) {
		sccMode = value;
		scc.setChipMode((value & 0x20) ? SCC::SCC_plusmode
					       : SCC::SCC_Compatible);
		invalidateMemCache(0x9800, 0x800);
		invalidateMemCache(0xB800, 0x800);
	}
	SCCEnable enable = getSCCEnable();
	bool isRamSegment2 = ((sccMode & 0x24) == 0x24) ||
			     ((sccMode & 0x10) == 0x10);
	bool isRamSegment3 = ((sccMode & 0x10) == 0x10);
	if (((enable == EN_SCC)     && !isRamSegment2 &&
	     (0x9800 <= addr) && (addr < 0xA000)) ||
	    ((enable == EN_SCCPLUS) && !isRamSegment3 &&
	     (0xB800 <= addr) && (addr < 0xC000))) {
		scc.writeMem(addr & 0xFF, value, time);
		return; // Pazos: when SCC registers are selected flashROM is not seen, so it does not accept commands.
	}

	if (isWritingKonamiBankRegisterDisabled() && (addr < 0x6000)) {
		dac.writeDAC(value, time);
	}

	unsigned page8kB = (addr >> 13) - 2;
	if (!areBankRegsAndOffsetRegsDisabled() && (page8kB < 4)) {
		// (possibly) write to bank registers
		// Konami-SCC
		if ((addr & 0x1800) == 0x1000) {
			// Storing 'sccBanks' may seem redundant at
			// first, but it's required to calculate
			// whether the SCC is enabled or not.
			sccBanks[page8kB] = value;
			if (isKonamiSCCmapperConfigured()) {
				// Masking of the mapper bits is done on
				// write (and only in Konami(-scc) mode)
				byte mask = areKonamiMapperLimitsEnabled() ? 0x3F : 0xFF;
				bankRegs[page8kB] = value & mask;
			}
			invalidateMemCache(0x4000 + 0x2000 * page8kB, 0x2000);
		}
		// Konami
		if (!isKonamiSCCmapperConfigured()) {
			if (isWritingKonamiBankRegisterDisabled() && (addr < 0x6000)) {
				// Switching 0x4000-0x5FFF disabled.
				// This bit blocks writing to the bank register
				// (an alternative was forcing a 0 on read).
				// It only has effect in Konami mode.
				// DAC is already handled above.
			} else {
				// Masking of the mapper bits is done on
				// write (and only in Konami(-scc) mode)
				if (!((addr < 0x5000) || ((0x5800 <= addr) && (addr < 0x6000))))
				{
					// only SCC range works
					byte mask = areKonamiMapperLimitsEnabled() ? 0x1F : 0xFF;
					bankRegs[page8kB] = value & mask;
					invalidateMemCache(0x4000 + 0x2000 * page8kB, 0x2000);
				}
			}
		}
	}

	if (flashAddr != unsigned(-1)) {
		writeToFlash(flashAddr, value);
	}
}

byte* KonamiUltimateCollection::getWriteCacheLine(word /*addr*/) const
{
	return nullptr; // flash isn't cacheable
}

template<typename Archive>
void KonamiUltimateCollection::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("flash", flash);
	ar.serialize("scc", scc);
	ar.serialize("DAC", dac);
	ar.serialize("sccMode", sccMode);
	ar.serialize("sccBanks", sccBanks);
	ar.serialize("mapperReg", mapperReg);
	ar.serialize("offsetReg", offsetReg);
	ar.serialize("bankRegs", bankRegs);
}
INSTANTIATE_SERIALIZE_METHODS(KonamiUltimateCollection);
REGISTER_MSXDEVICE(KonamiUltimateCollection, "KonamiUltimateCollection");

} // namespace openmsx
