#include "MegaFlashRomSCCPlus.hh"
#include "DummyAY8910Periphery.hh"
#include "MSXCPUInterface.hh"
#include "CacheLine.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <array>
#include <cassert>

/******************************************************************************
 * DOCUMENTATION AS PROVIDED BY MANUEL PAZOS, WHO DEVELOPED THE CARTRIDGE     *
 ******************************************************************************

--------------------------------------------------------------------------------
MegaFlashROM SCC+ Technical Details
(c) Manuel Pazos 28-09-2010
--------------------------------------------------------------------------------

Main features:
 - 1024KB flashROM memory (M29W800DB)
 - SCC-I (2312P001)
 - PSG (AY-3-8910/YM2149)
 - Mappers: ASCII8, ASCII16, Konami, Konami SCC, linear 64K
 - Subslot simulation (4 x 256K)


--------------------------------------------------------------------------------
[Memory]

 - Model Numonyx M29W800DB TSOP48
 - Datasheet: https://media-www.micron.com/-/media/client/global/documents/products/data-sheet/nor-flash/parallel/m29w/m29w800d.pdf
 - Block layout:
     #00000 16K
     #04000  8K
     #06000  8K
     #08000 32K
     #10000 64K x 15
 - Command addresses #4555 and #5AAA
 - Commands:
    AUTOSELECT  #90
    WRITE       #A0
    CHIP_ERASE  #10
    BLOCK_ERASE #30
    RESET       #F0
 - FlashROM ID = #5B


--------------------------------------------------------------------------------
[PSG]

 The PSG included in the cartridge is mapped to ports #10-#12

 Port #A0 -> #10
 Port #A1 -> #11
 Port #A2 -> #12

 The PSG is read only.

--------------------------------------------------------------------------------
[REGISTERS]

 -#7FFE or #7FFF = CONFIGURATION REGISTER:
    7 mapper mode 1: \ %00 = SCC,  %01 = 64K
    6 mapper mode 0: / %10 = ASC8, %11 = ASC16
    5 mapper mode  :   Select Konami mapper (0 = SCC, 1 = normal)
    4 Enable subslot mode and register #FFFF (1 = Enable, 0 = Disable)
    3 Disable #4000-#5FFF mapper in Konami mode (0 = Enable, 1 = Disable)
    2 Disable configuration register (1 = Disabled)
    1 Disable mapper registers (0 = Enable, 1 = Disable)
    0 Enable 512K mapper limit in SCC mapper or 256K limit in Konami mapper
(1 = Enable, 0 = Disable)


--------------------------------------------------------------------------------
[MAPPERS]

 - ASCII 8:    Common ASC8 mapper

 - ASCII 16:   Common ASC16 mapper

 - Konami:     Common Konami mapper.
               Bank0 (#4000-#5FFF) can be also changed unless CONF_REG bit3 is 1

 - Konami SCC: Common Konami SCC mapper
               Bank0 mapper register: if bank number bit 7 is 1 then bit 6-0 sets mapper offset.

 - Linear 64:  #0000-#3FFF bank0
               #4000-#7FFF bank1
               #8000-#BFFF bank2
               #C000-#FFFF bank3
               Banks mapper registers addresses = Konami

--------------------------------------------------------------------------------
[DEFAULT VALUES]
 - CONFIGURATION REGISTER = 0
 - Bank0 = 0
 - Bank1 = 1
 - Bank2 = 2
 - Bank3 = 3
 - Bank offset = 0
 - Subslot register = 0

--------------------------------------------------------------------------------
[LOGIC]

  Banks0-3: are set depending on the mapper mode.
  Subslots: are set writing to #FFFF in the MegaFlashROM SCC+ slot when CONFIG_REG bit 4 is set.
  Offset:   is set by writing offset value + #80 to bank0 mapper register in Konami SCC mode.

  -- Subslots offsets
  SubOff0 = subslot(1-0) & "0000" + mapOff;	-- In 64K mode, the banks are 16K, with the offsets halved
  SubOff1 = subslot(3-2) & "0000" + mapOff when maptyp = "10" else subslot(3-2) & "00000" + mapOff;
  SubOff2 = subslot(5-4) & "0000" + mapOff when maptyp = "10" else subslot(5-4) & "00000" + mapOff;
  SubOff3 = subslot(7-6) & "0000" + mapOff when maptyp = "10" else subslot(7-6) & "00000";

  -- Calculate the bank mapper to use taking into account the offset
  Bank0 =  SubBank0(x) + SubOff0 when SccModeA(4) = '1' and subslot(1 downto 0) = "00" and maptyp = "10" else
           SubBank0(1) + SubOff0 when SccModeA(4) = '1' and subslot(1 downto 0) = "01" and maptyp = "10" else
           SubBank0(2) + SubOff0 when SccModeA(4) = '1' and subslot(1 downto 0) = "10" and maptyp = "10" else
           SubBank0(3) + SubOff0 when SccModeA(4) = '1' and subslot(1 downto 0) = "11" and maptyp = "10" else

           SubBank0(0) + SubOff1 when SccModeA(4) = '1' and subslot(3 downto 2) = "00" else
           SubBank0(1) + SubOff1 when SccModeA(4) = '1' and subslot(3 downto 2) = "01" else
           SubBank0(2) + SubOff1 when SccModeA(4) = '1' and subslot(3 downto 2) = "10" else
           SubBank0(3) + SubOff1 when SccModeA(4) = '1' and subslot(3 downto 2) = "11" else
           SccBank0    + mapOff;

  Bank1 <= SubBank1(0) + SubOff1 when SccModeA(4) = '1' and subslot(3 downto 2) = "00" else
           SubBank1(1) + SubOff1 when SccModeA(4) = '1' and subslot(3 downto 2) = "01" else
           SubBank1(2) + SubOff1 when SccModeA(4) = '1' and subslot(3 downto 2) = "10" else
           SubBank1(3) + SubOff1 when SccModeA(4) = '1' and subslot(3 downto 2) = "11" else
           SccBank1    + mapOff;

  Bank2 <= SubBank2(0) + SubOff2 when SccModeA(4) = '1' and subslot(5 downto 4) = "00" else
           SubBank2(1) + SubOff2 when SccModeA(4) = '1' and subslot(5 downto 4) = "01" else
           SubBank2(2) + SubOff2 when SccModeA(4) = '1' and subslot(5 downto 4) = "10" else
           SubBank2(3) + SubOff2 when SccModeA(4) = '1' and subslot(5 downto 4) = "11" else
           SccBank2    + mapOff;

  Bank3 <= SubBank3(0) + SubOff3 when SccModeA(4) = '1' and subslot(7 downto 6) = "00" and maptyp = "10" else
           SubBank3(1) + SubOff3 when SccModeA(4) = '1' and subslot(7 downto 6) = "01" and maptyp = "10" else
           SubBank3(2) + SubOff3 when SccModeA(4) = '1' and subslot(7 downto 6) = "10" and maptyp = "10" else
           SubBank3(3) + SubOff3 when SccModeA(4) = '1' and subslot(7 downto 6) = "11" and maptyp = "10" else

           SubBank3(0) + SubOff2 when SccModeA(4) = '1' and subslot(5 downto 4) = "00" else
           SubBank3(1) + SubOff2 when SccModeA(4) = '1' and subslot(5 downto 4) = "01" else
           SubBank3(2) + SubOff2 when SccModeA(4) = '1' and subslot(5 downto 4) = "10" else
           SubBank3(3) + SubOff2 when SccModeA(4) = '1' and subslot(5 downto 4) = "11" else
           SccBank3    + mapOff;


            -- 64K Mode
  RamAdr <= Bank0(5 downto 0) & adr(13 downto 0) when adr(15 downto 14) = "00" and maptyp = "10" else 	--#0000-#3FFF
            Bank1(5 downto 0) & adr(13 downto 0) when adr(15 downto 14) = "01" and maptyp = "10" else 	--#4000-#7FFF
            Bank2(5 downto 0) & adr(13 downto 0) when adr(15 downto 14) = "10" and maptyp = "10" else 	--#8000-#BFFF
            Bank3(5 downto 0) & adr(13 downto 0) when adr(15 downto 14) = "11" and maptyp = "10" else 	--#C000-#FFFF
            -- Modes SCC, ASC8 and ASC16
            Bank0(6 downto 0) & adr(12 downto 0) when adr(14 downto 13) = "10" else 	--#4000-#5FFF
            Bank1(6 downto 0) & adr(12 downto 0) when adr(14 downto 13) = "11" else 	--#6000-#7FFF
            Bank2(6 downto 0) & adr(12 downto 0) when adr(14 downto 13) = "00" else 	--#8000-#9FFF
            Bank3(6 downto 0) & adr(12 downto 0);

--------------------------------------------------------------------------------
[FLASH ROM ADDRESSING]

 In order to be compatible with OPFX which sends command sequences like
 (0x555) = 0xAA, (0xAAA) = 0x55, whilst dual mode 8/16-bit chips like the
 M29W800DB expect (0xAAA) = 0xAA, (0x555) = 0x55 when operating in byte mode,
 CPU line A12 is connected to Flash line A0.

  CPU line | Flash line (-1 on datasheet)
 ----------+------------------------------
  A0-A11   |  A1-A12
  A12      |  A0
  A13-A15  |  A13-A19 (through mapper)

 Since the smallest sector size is 8 kB, sector numbers are unaffected, only
 sector offsets are. Unfortunately, this prohibits read caching.

******************************************************************************/

namespace openmsx {

MegaFlashRomSCCPlus::MegaFlashRomSCCPlus(
		const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, scc("MFR SCC+ SCC-I", config, getCurrentTime(), SCC::Mode::Compatible)
	, psg("MFR SCC+ PSG", DummyAY8910Periphery::instance(), config,
	      getCurrentTime())
	, flash(rom, AmdFlashChip::M29W800DB, {}, config)
{
	powerUp(getCurrentTime());
	getCPUInterface().register_IO_Out_range(0x10, 2, this);
}

MegaFlashRomSCCPlus::~MegaFlashRomSCCPlus()
{
	getCPUInterface().unregister_IO_Out_range(0x10, 2, this);
}

void MegaFlashRomSCCPlus::powerUp(EmuTime::param time)
{
	scc.powerUp(time);
	reset(time);
}

void MegaFlashRomSCCPlus::reset(EmuTime::param time)
{
	configReg = 0;
	offsetReg = 0;
	subslotReg = 0;
	for (auto& regs : bankRegs) {
		ranges::iota(regs, byte(0));
	}

	sccMode = 0;
	ranges::iota(sccBanks, byte(0));
	scc.reset(time);

	psgLatch = 0;
	psg.reset(time);

	flash.reset();

	invalidateDeviceRCache(); // flush all to be sure
}

MegaFlashRomSCCPlus::SCCEnable MegaFlashRomSCCPlus::getSCCEnable() const
{
	if ((sccMode & 0x20) && (sccBanks[3] & 0x80)) {
		return EN_SCCPLUS;
	} else if ((!(sccMode & 0x20)) && ((sccBanks[2] & 0x3F) == 0x3F)) {
		return EN_SCC;
	} else {
		return EN_NONE;
	}
}

unsigned MegaFlashRomSCCPlus::getSubslot(unsigned addr) const
{
	return (configReg & 0x10)
	     ? (subslotReg >> (2 * (addr >> 14))) & 0x03
	     : 0;
}

unsigned MegaFlashRomSCCPlus::getFlashAddr(unsigned addr) const
{
	// Pazos: FlashROM A0 (A-1 on datasheet) is connected to CPU A12.
	addr = (addr & 0xE000) | ((addr & 0x1000) >> 12) | ((addr & 0x0FFF) << 1);

	unsigned subslot = getSubslot(addr);
	unsigned tmp = [&] {
		if ((configReg & 0xC0) == 0x40) {
			unsigned bank = bankRegs[subslot][addr >> 14] + offsetReg;
			return (bank * 0x4000) + (addr & 0x3FFF);
		} else {
			unsigned page = (addr >> 13) - 2;
			if (page >= 4) {
				// Bank: -2, -1, 4, 5. So not mapped in this region,
				// returned value should not be used. But querying it
				// anyway is easier, see start of writeMem().
				return unsigned(-1);
			}
			unsigned bank = bankRegs[subslot][page] + offsetReg;
			return (bank * 0x2000) + (addr & 0x1FFF);
		}
	}();
	return ((0x40000 * subslot) + tmp) & 0xFFFFF; // wrap at 1MB
}

byte MegaFlashRomSCCPlus::peekMem(word addr, EmuTime::param time) const
{
	if ((configReg & 0x10) && (addr == 0xFFFF)) {
		// read subslot register
		return subslotReg ^ 0xFF;
	}

	if ((configReg & 0xE0) == 0x00) {
		SCCEnable enable = getSCCEnable();
		if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
		    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
			return scc.peekMem(narrow_cast<uint8_t>(addr & 0xFF), time);
		}
	}

	if (((configReg & 0xC0) == 0x40) ||
	    ((0x4000 <= addr) && (addr < 0xC000))) {
		// read (flash)rom content
		unsigned flashAddr = getFlashAddr(addr);
		assert(flashAddr != unsigned(-1));
		return flash.peek(flashAddr);
	} else {
		// unmapped read
		return 0xFF;
	}
}

byte MegaFlashRomSCCPlus::readMem(word addr, EmuTime::param time)
{
	if ((configReg & 0x10) && (addr == 0xFFFF)) {
		// read subslot register
		return subslotReg ^ 0xFF;
	}

	if ((configReg & 0xE0) == 0x00) {
		SCCEnable enable = getSCCEnable();
		if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
		    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
			return scc.readMem(narrow_cast<uint8_t>(addr & 0xFF), time);
		}
	}

	if (((configReg & 0xC0) == 0x40) ||
	    ((0x4000 <= addr) && (addr < 0xC000))) {
		// read (flash)rom content
		unsigned flashAddr = getFlashAddr(addr);
		assert(flashAddr != unsigned(-1));
		return flash.read(flashAddr);
	} else {
		// unmapped read
		return 0xFF;
	}
}

const byte* MegaFlashRomSCCPlus::getReadCacheLine(word addr) const
{
	if ((configReg & 0x10) &&
	    ((addr & CacheLine::HIGH) == (0xFFFF & CacheLine::HIGH))) {
		// read subslot register
		return nullptr;
	}

	if ((configReg & 0xE0) == 0x00) {
		SCCEnable enable = getSCCEnable();
		if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
		    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
			return nullptr;
		}
	}

	if (((configReg & 0xC0) == 0x40) ||
	    ((0x4000 <= addr) && (addr < 0xC000))) {
		return nullptr; // uncacheable due to address swizzling
	} else {
		// unmapped read
		return unmappedRead.data();
	}
}

void MegaFlashRomSCCPlus::writeMem(word addr, byte value, EmuTime::param time)
{
	// address is calculated before writes to other regions take effect
	unsigned flashAddr = getFlashAddr(addr);

	// There are several overlapping functional regions in the address
	// space. A single write can trigger behaviour in multiple regions. In
	// other words there's no priority amongst the regions where a higher
	// priority region blocks the write from the lower priority regions.
	if ((configReg & 0x10) && (addr == 0xFFFF)) {
		// write subslot register
		byte diff = value ^ subslotReg;
		subslotReg = value;
		for (auto i : xrange(4)) {
			if (diff & (3 << (2 * i))) {
				invalidateDeviceRCache(0x4000 * i, 0x4000);
			}
		}
	}

	if (((configReg & 0x04) == 0x00) && ((addr & 0xFFFE) == 0x7FFE)) {
		// write config register
		configReg = value;
		invalidateDeviceRCache(); // flush all to be sure
	}

	if ((configReg & 0xE0) == 0x00) {
		// Konami-SCC
		if ((addr & 0xFFFE) == 0xBFFE) {
			sccMode = value;
			scc.setMode((value & 0x20) ? SCC::Mode::Plus
			                           : SCC::Mode::Compatible);
			invalidateDeviceRCache(0x9800, 0x800);
			invalidateDeviceRCache(0xB800, 0x800);
		}
		SCCEnable enable = getSCCEnable();
		bool isRamSegment2 = ((sccMode & 0x24) == 0x24) ||
		                     ((sccMode & 0x10) == 0x10);
		bool isRamSegment3 = ((sccMode & 0x10) == 0x10);
		if (((enable == EN_SCC)     && !isRamSegment2 &&
		     (0x9800 <= addr) && (addr < 0xA000)) ||
		    ((enable == EN_SCCPLUS) && !isRamSegment3 &&
		     (0xB800 <= addr) && (addr < 0xC000))) {
			scc.writeMem(narrow_cast<uint8_t>(addr & 0xFF), value, time);
			return; // Pazos: when SCC registers are selected flashROM is not seen, so it does not accept commands.
		}
	}

	unsigned subslot = getSubslot(addr);
	unsigned page8kB = (addr >> 13) - 2;
	if (((configReg & 0x02) == 0x00) && (page8kB < 4)) {
		// (possibly) write to bank registers
		switch (configReg & 0xE0) {
		case 0x00:
			// Konami-SCC
			if ((addr & 0x1800) == 0x1000) {
				// Storing 'sccBanks' may seem redundant at
				// first, but it's required to calculate
				// whether the SCC is enabled or not.
				sccBanks[page8kB] = value;
				if ((value & 0x80) && (page8kB == 0)) {
					offsetReg = value & 0x7F;
					invalidateDeviceRCache(0x4000, 0x8000);
				} else {
					// Masking of the mapper bits is done on
					// write (and only in Konami(-scc) mode)
					byte mask = (configReg & 0x01) ? 0x3F : 0x7F;
					bankRegs[subslot][page8kB] = value & mask;
					invalidateDeviceRCache(0x4000 + 0x2000 * page8kB, 0x2000);
				}
			}
			break;
		case 0x20: {
			// Konami
			if (((configReg & 0x08) == 0x08) && (addr < 0x6000)) {
				// Switching 0x4000-0x5FFF disabled.
				// This bit blocks writing to the bank register
				// (an alternative was forcing a 0 on read).
				// It only has effect in Konami mode.
				break;
			}
			// Making of the mapper bits is done on
			// write (and only in Konami(-scc) mode)
			if ((addr < 0x5000) || ((0x5800 <= addr) && (addr < 0x6000))) break; // only SCC range works
			byte mask = (configReg & 0x01) ? 0x1F : 0x7F;
			bankRegs[subslot][page8kB] = value & mask;
			invalidateDeviceRCache(0x4000 + 0x2000 * page8kB, 0x2000);
			break;
		}
		case 0x40:
		case 0x60:
			// 64kB
			bankRegs[subslot][page8kB] = value;
			invalidateDeviceRCache(0x0000 + 0x4000 * page8kB, 0x4000);
			break;
		case 0x80:
		case 0xA0:
			// ASCII-8
			if ((0x6000 <= addr) && (addr < 0x8000)) {
				byte bank = (addr >> 11) & 0x03;
				bankRegs[subslot][bank] = value;
				invalidateDeviceRCache(0x4000 + 0x2000 * bank, 0x2000);
			}
			break;
		case 0xC0:
		case 0xE0:
			// ASCII-16
			// This behaviour is confirmed by Manuel Pazos (creator
			// of the cartridge): ASCII-16 uses all 4 bank registers
			// and one bank switch changes 2 registers at once.
			// This matters when switching mapper mode, because
			// the content of the bank registers is unchanged after
			// a switch.
			if ((0x6000 <= addr) && (addr < 0x6800)) {
				bankRegs[subslot][0] = narrow_cast<uint8_t>(2 * value + 0);
				bankRegs[subslot][1] = narrow_cast<uint8_t>(2 * value + 1);
				invalidateDeviceRCache(0x4000, 0x4000);
			}
			if ((0x7000 <= addr) && (addr < 0x7800)) {
				bankRegs[subslot][2] = narrow_cast<uint8_t>(2 * value + 0);
				bankRegs[subslot][3] = narrow_cast<uint8_t>(2 * value + 1);
				invalidateDeviceRCache(0x8000, 0x4000);
			}
			break;
		}
	}

	// write to flash
	if (((configReg & 0xC0) == 0x40) ||
	    ((0x4000 <= addr) && (addr < 0xC000))) {
		assert(flashAddr != unsigned(-1));
		return flash.write(flashAddr, value);
	}
}

byte* MegaFlashRomSCCPlus::getWriteCacheLine(word /*addr*/)
{
	return nullptr;
}


void MegaFlashRomSCCPlus::writeIO(word port, byte value, EmuTime::param time)
{
	if ((port & 0xFF) == 0x10) {
		psgLatch = value & 0x0F;
	} else {
		assert((port & 0xFF) == 0x11);
		psg.writeRegister(psgLatch, value, time);
	}
}


template<typename Archive>
void MegaFlashRomSCCPlus::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("scc",        scc,
	             "psg",        psg,
	             "flash",      flash,

	             "configReg",  configReg,
	             "offsetReg",  offsetReg,
	             "subslotReg", subslotReg,
	             "bankRegs",   bankRegs,
	             "psgLatch",   psgLatch,
	             "sccMode",    sccMode,
	             "sccBanks",   sccBanks);
}
INSTANTIATE_SERIALIZE_METHODS(MegaFlashRomSCCPlus);
REGISTER_MSXDEVICE(MegaFlashRomSCCPlus, "MegaFlashRomSCCPlus");

} // namespace openmsx
