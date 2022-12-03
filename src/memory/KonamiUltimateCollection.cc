#include "KonamiUltimateCollection.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "serialize.hh"
#include <array>

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
4     Flash write enable
3     Disable #4000-#5FFF mapper in Konami mode, Enable DAC (works like the DAC of Synthesizer or Majutsushi)
2     Disable mapper register
1     Disable mapper (bank switching)
0     no function anymore (was mapper limits)

[1] bit 5 only changes the address range of the mapper (Konami or Konami SCC)
but the SCC is always available.  This feature is inherited from MFC SCC+ subslot
mode because all subslots share the same mapper type. So to make Konami
combinations (i.e.: Konami ROM with Konami SCC ROM) a "common mapper" is needed
and the SCC must be available. So I made a "Konami mapper" with SCC.  In fact,
all Konami and Konami SCC ROMs should work with "Konami" mapper in KUC.

******************************************************************************/

namespace openmsx {

static constexpr auto sectorInfo = [] {
	// 8 * 8kB, followed by 127 * 64kB
	using Info = AmdFlash::SectorInfo;
	std::array<Info, 8 + 127> result = {};
	std::fill(result.begin(), result.begin() + 8, Info{ 8 * 1024, false});
	std::fill(result.begin() + 8, result.end(),   Info{64 * 1024, false});
	return result;
}();

KonamiUltimateCollection::KonamiUltimateCollection(
		const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, flash(rom, sectorInfo, 0x207E,
	        AmdFlash::Addressing::BITS_12, config)
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
	sccMode = 0;
	ranges::iota(bankRegs, byte(0));

	scc.reset(time);
	dac.reset(time);

	invalidateDeviceRWCache(); // flush all to be sure
}

unsigned KonamiUltimateCollection::getFlashAddr(unsigned addr) const
{
	unsigned page8kB = (addr >> 13) - 2;
	if (page8kB >= 4) return unsigned(-1); // outside [0x4000, 0xBFFF]

	byte bank = bankRegs[page8kB] + offsetReg; // wrap at 8 bit
	return ((mapperReg & 0xC0) << (21 - 6)) | (bank << 13) | (addr & 0x1FFF);
}

bool KonamiUltimateCollection::isSCCAccess(word addr) const
{
	if (sccMode & 0x10) return false;

	if (addr & 0x0100) {
		// Address bit 8 must be zero, this is different from a real
		// SCC/SCC+. According to Manuel Pazos this is a leftover from
		// an earlier version that had 2 SCCs: the SCC on the left or
		// right channel reacts when address bit 8 is respectively 0/1.
		return false;
	}

	if (sccMode & 0x20) {
		// SCC+   range: 0xB800..0xBFFF,  excluding 0xBFFE-0xBFFF
		return  (bankRegs[3] & 0x80)          && (0xB800 <= addr) && (addr < 0xBFFE);
	} else {
		// SCC    range: 0x9800..0x9FFF,  excluding 0x9FFE-0x9FFF
		return ((bankRegs[2] & 0x3F) == 0x3F) && (0x9800 <= addr) && (addr < 0x9FFE);
	}
}

byte KonamiUltimateCollection::readMem(word addr, EmuTime::param time)
{
	if (isSCCAccess(addr)) {
		return scc.readMem(narrow_cast<uint8_t>(addr & 0xFF), time);
	}

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.read(flashAddr)
		: 0xFF; // unmapped read
}

byte KonamiUltimateCollection::peekMem(word addr, EmuTime::param time) const
{
	if (isSCCAccess(addr)) {
		return scc.peekMem(narrow_cast<uint8_t>(addr & 0xFF), time);
	}

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.peek(flashAddr)
		: 0xFF; // unmapped read
}

const byte* KonamiUltimateCollection::getReadCacheLine(word addr) const
{
	if (isSCCAccess(addr)) return nullptr;

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.getReadCacheLine(flashAddr)
		: unmappedRead.data();
}

void KonamiUltimateCollection::writeMem(word addr, byte value, EmuTime::param time)
{
	unsigned page8kB = (addr >> 13) - 2;
	if (page8kB >= 4) return; // outside [0x4000, 0xBFFF]

	// There are several overlapping functional regions in the address
	// space. A single write can trigger behaviour in multiple regions. In
	// other words there's no priority amongst the regions where a higher
	// priority region blocks the write from the lower priority regions.
	// This only goes for places where the flash is 'seen', so not for the
	// SCC registers

	if (isSCCAccess(addr)) {
		scc.writeMem(narrow_cast<uint8_t>(addr & 0xFF), value, time);
		return; // write to SCC blocks write to other functions
	}

	// address is calculated before writes to other regions take effect
	unsigned flashAddr = getFlashAddr(addr);

	// Mapper and offset registers
	if (isMapperRegisterEnabled()) {
		if (addr == 0x7FFF) {
			mapperReg = value;
		} else if (addr == 0x7FFE) {
			offsetReg = value;
		}
		invalidateDeviceRCache(); // flush all to be sure
	}


	// DAC
	if (isBank0Disabled() && (addr < 0x6000) && ((addr & 0x0010) == 0)) {
		dac.writeDAC(value, time);
	}

	if (areBankRegsEnabled()) {
		// Bank-switching
		if (isKonamiSCCmode()) {
			// Konami-SCC
			if ((addr & 0x1800) == 0x1000) {
				// [0x5000,0x57FF] [0x7000,0x77FF]
				// [0x9000,0x97FF] [0xB000,0xB7FF]
				// Masking of the mapper bits is done on write
				bankRegs[page8kB] = value;
				invalidateDeviceRCache(0x4000 + 0x2000 * page8kB, 0x2000);
			}
		} else {
			// Konami
			if (isBank0Disabled() && (addr < 0x6000)) {
				// Switching 0x4000-0x5FFF disabled (only Konami mode).
			} else {
				// [0x5000,0x57FF] asymmetric!!!
				// [0x6000,0x7FFF] [0x8000,0x9FFF] [0xA000,0xBFFF]
				if (!((addr < 0x5000) || ((0x5800 <= addr) && (addr < 0x6000)))) {
					// Masking of the mapper bits is done on write
					bankRegs[page8kB] = value;
					invalidateDeviceRCache(0x4000 + 0x2000 * page8kB, 0x2000);
				}
			}
		}

		// SCC mode register
		if ((addr & 0xFFFE) == 0xBFFE) {
			sccMode = value;
			scc.setChipMode((value & 0x20) ? SCC::SCC_plusmode
						       : SCC::SCC_Compatible);
			invalidateDeviceRCache(0x9800, 0x800);
			invalidateDeviceRCache(0xB800, 0x800);
		}
	}

	if ((flashAddr != unsigned(-1)) && isFlashRomWriteEnabled()) {
		flash.write(flashAddr, value);
	}
}

byte* KonamiUltimateCollection::getWriteCacheLine(word addr) const
{
	return ((0x4000 <= addr) && (addr < 0xC000))
	       ? nullptr        // [0x4000,0xBFFF] isn't cacheable
	       : unmappedWrite.data();
}

template<typename Archive>
void KonamiUltimateCollection::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("flash",     flash,
	             "scc",       scc,
	             "DAC",       dac,
	             "mapperReg", mapperReg,
	             "offsetReg", offsetReg,
	             "sccMode",   sccMode,
	             "bankRegs",  bankRegs);
}
INSTANTIATE_SERIALIZE_METHODS(KonamiUltimateCollection);
REGISTER_MSXDEVICE(KonamiUltimateCollection, "KonamiUltimateCollection");

} // namespace openmsx
