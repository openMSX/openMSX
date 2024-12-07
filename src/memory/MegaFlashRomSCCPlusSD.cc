#include "MegaFlashRomSCCPlusSD.hh"
#include "DummyAY8910Periphery.hh"
#include "MSXCPUInterface.hh"
#include "CacheLine.hh"
#include "CheckedRam.hh"
#include "SdCard.hh"
#include "enumerate.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <array>
#include <memory>

/******************************************************************************
 * DOCUMENTATION AS PROVIDED BY MANUEL PAZOS, WHO DEVELOPED THE CARTRIDGE     *
 ******************************************************************************

--------------------------------------------------------------------------------
MegaFlashROM SCC+ SD Technical Details
(c) Manuel Pazos 24-02-2014
--------------------------------------------------------------------------------

[Main features]

 - 8192 KB flashROM memory
 - SD interface (MegaSD)
 - SCC-I (2312P001)
 - PSG (AY-3-8910/YM2149)
 - Mappers: ASCII8, ASCII16, Konami, Konami SCC, linear 64K
 - Slot expander


--------------------------------------------------------------------------------
[Memory]

 - Model Numonyx / Micron M29W640FB / M29W640GB TSOP48
 - Datasheet: https://media-www.micron.com/-/media/client/global/documents/products/data-sheet/nor-flash/parallel/m29w/m29w640f.pdf
              https://media-www.micron.com/-/media/client/global/documents/products/data-sheet/nor-flash/parallel/m29w/m29w640g.pdf
 - Block layout:
     #00000 8K x 8
     #10000 64K x 127
 - Command addresses:
     #4555 and #4AAA
 - FlashROM ID:
    ID_M29W640FB    #FD
    ID_M29W640GB    #7E 10 00


--------------------------------------------------------------------------------
[PSG]

 The PSG included in the cartridge is mapped to ports #10-#12

 Port #A0 -> #10
 Port #A1 -> #11
 Port #A2 -> #12

 The PSG is read only.

--------------------------------------------------------------------------------
[Cartridge layout]

    - Subslot 0: Recovery - 16K linear (Visible on page 1 and 2)
    - Subslot 1: MegaFlashROM SCC+ - 7104K (multiple mapper support)
    - Subslot 2: RAM (when available)
    - Subslot 3: MegaSD - 1024K (ASC8 mapper)


--------------------------------------------------------------------------------
[FlashROM layout]

#000000+----------------+
       |    Recovery    | 16K - Subslot 0 (Note: Blocks are write protected
by VPP/WD pin)
       +----------------+
       |   DSK Kernel   | 16K
       +----------------+
       |    Not used    | 32K
#010000+----------------+
       |                |
       |                |
       | MegaFlashROM   | 7104K - Subslot 1
       |                |
       |                |
#700000+----------------+
       |                |
       |     MegaSD     | 1024K - Subslot 3
       |                |
       +----------------+


--------------------------------------------------------------------------------
[Subslot register (#FFFF)]

    Available when writing at #FFFF in the cartridge slot.
    Reading that address will return all the bits inverted.
    Default value = 0


--------------------------------------------------------------------------------
[Subslot 0: RECOVERY]

    Size 16K
    Common ROM (Without mapper).
    Visible on pages 1 and 2 (mirrored all over the slot)


--------------------------------------------------------------------------------
[Subslot 1: MegaFlashROM SCC+ SD]

  [REGISTERS]

    [MAPPER REGISTER (#7FFF)]
    7	mapper mode 1: \ #00 = SCC,  #40 = 64K
    6	mapper mode 0: / #80 = ASC8, #C0 = ASC16
    5	mapper mode  :   Select Konami mapper (0=SCC or 1=normal)
    4
    3	Disable #4000-#5FFF mapper in Konami mode
    2	Disable this mapper register #7FFF
    1	Disable mapper and offset registers
    0	Enable 512K mapper limit in SCC mapper or 256K limit in Konami mapper


    [OFFSET REGISTER (#7FFD)]
    7-0 Offset value bits 7-0


    [OFFSET REGISTER (#7FFE)]
    1	Offset bit 9
    0	Offset bit 8


    [CONFIG REGISTER (#7FFC)]
    7	Disable config register (1 = Disabled)
    6
    5	Disable SRAM (i.e. the RAM in subslot 2)
    4	DSK mode (1 = On): Bank 0 and 1 are remapped to DSK kernel (config banks 2-3)
    3	Cartridge PSG also mapped to ports #A0-#A3
    2	Subslots disabled (1 = Disabled) Only MegaFlashROM SCC+ is available.
    1	FlashROM Block protect (1 = Protect) VPP_WD pin
    0	FlashROM write enable (1 = Enabled)



  [MAPPERS]

   - ASCII 8:    Common ASC8 mapper

   - ASCII 16:   Common ASC16 mapper

   - Konami:     Common Konami mapper.
                 Bank0 (#4000-#5FFF) can be also changed unless [MAPPER REGISTER] bit 3 is 1

   - Konami SCC: Common Konami SCC mapper

   - Linear 64:  #0000-#3FFF bank0
                 #4000-#7FFF bank1
                 #8000-#BFFF bank2
                 #C000-#FFFF bank3
                 Banks mapper registers addresses = Konami


  [DEFAULT VALUES]

   - MAPPER REGISTER = 0
   - CONFIG REGISTER = %00000011
   - MapperBank0 = 0
   - MapperBank1 = 1
   - MapperBank2 = 2
   - MapperBank3 = 3
   - BankOffset = 0
   - Subslot register = 0


  [LOGIC]

  Bank0   <=    "1111111010" when CONFIG REGISTER(4) = '1' and MapperBank0 = "0000000000" else  ; DSK mode
                MapperBank0 + bankOffset;

  Bank1   <=    "1111111011" when CONFIG REGISTER(4) = '1' and MapperBank1 = "0000000001" else  ; DSK mode
                MapperBank1 + bankOffset;

  Bank2   <=    MapperBank2 + bankOffset;

  Bank3   <=    MapperBank3 + bankOffset;


  RamAdr  <=
                -- Mapper in 64K mode
                Bank0(8 downto 0) & adr(13 downto 0) when adr(15 downto 14) = "00" else  --#0000-#3FFF
                Bank1(8 downto 0) & adr(13 downto 0) when adr(15 downto 14) = "01" else  --#4000-#7FFF
                Bank2(8 downto 0) & adr(13 downto 0) when adr(15 downto 14) = "10" else  --#8000-#BFFF
                Bank3(8 downto 0) & adr(13 downto 0) when adr(15 downto 14) = "11" else  --#C000-#FFFF

                -- Mapper in SCC, ASC8 or ASC16 modes
                Bank0(9 downto 0) & adr(12 downto 0) when adr(14 downto 13) = "10" else  --#4000-#5FFF
                Bank1(9 downto 0) & adr(12 downto 0) when adr(14 downto 13) = "11" else  --#6000-#7FFF
                Bank2(9 downto 0) & adr(12 downto 0) when adr(14 downto 13) = "00" else  --#8000-#9FFF
                Bank3(9 downto 0) & adr(12 downto 0);                                    --#A000-#BFFF


  Note: It is possible to access the whole flashROM from the MegaFlashROM SCC+
  SD using the offsets register!

--------------------------------------------------------------------------------
[Subslot 2: 512K RAM expansion]

  Mapper ports:
    Page 0 = #FC
    Page 1 = #FD
    Page 2 = #FE
    Page 3 = #FF

  Default bank values:
    Page 0 = 3
    Page 1 = 2
    Page 2 = 1
    Page 3 = 0

  Disabled when [CONFIG REGISTER] bit 5 = 1

  Since mapper ports must not be read, as stated on MSX Technical Handbook, and
  mapper ports as read only, as stated on MSX Datapack, all read operations on
  these ports will not return any value.

UPDATE:
  Initial version indeed did not support reading the memory mapper ports. But
  upon user request this feature was added later.

--------------------------------------------------------------------------------
[Subslot 3: MegaSD]

  Mapper type: ASCII8

  Default mapper values:
    Bank0 = 0
    Bank1 = 1
    Bank2 = 0
    Bank3 = 0

  Memory range 1024K: Banks #00-#7F are mirrored in #80-#FF (except registers
  bank #40-#7F)

  Memory registers area (Bank #40-#7F):
    #4000-#57FF: SD card access (R/W)
                 #4000-#4FFF: /CS signal = 0 - SD enabled
                 #5000-#5FFF: /CS signal = 1 - SD disabled

    #5800-#5FFF: SD slot select (bit 0: 0 = SD slot 1, 1 = SD slot 2)

  Writes to these registers do not write through to the FlashROM.

  Cards work in SPI mode.
  Signals used: CS, DI, DO, SCLK
  When reading, 8 bits are read from DO
  When writing, 8 bits are written to DI

  SD specifications: https://www.sdcard.org/downloads/pls/simplified_specs/part1_410.pdf

******************************************************************************/

static constexpr unsigned MEMORY_MAPPER_SIZE = 512;
static constexpr uint8_t MEMORY_MAPPER_MASK = (MEMORY_MAPPER_SIZE / 16) - 1;

namespace openmsx {

MegaFlashRomSCCPlusSD::MegaFlashRomSCCPlusSD(const DeviceConfig& config)
	: MSXDevice(config)
	, flash("MFR SCC+ SD flash", AmdFlashChip::M29W640GB, {}, config)
	, scc("MFR SCC+ SD SCC-I", config, getCurrentTime(), SCC::Mode::Compatible)
	, psg("MFR SCC+ SD PSG", DummyAY8910Periphery::instance(), config,
	      getCurrentTime())
	, checkedRam(config.getChildDataAsBool("hasmemorymapper", true) ?
		std::make_unique<CheckedRam>(config, getName() + " memory mapper", "memory mapper", MEMORY_MAPPER_SIZE * 1024)
		: nullptr)
	, mapperIO(checkedRam ? std::make_unique<MapperIO>(*this) : nullptr) // handles ports 0xfc-0xff
{
	powerUp(getCurrentTime());
	getCPUInterface().register_IO_Out_range(0x10, 2, this);

	sdCard[0] = std::make_unique<SdCard>(DeviceConfig(config, config.findChild("sdcard1")));
	sdCard[1] = std::make_unique<SdCard>(DeviceConfig(config, config.findChild("sdcard2")));
}

MegaFlashRomSCCPlusSD::~MegaFlashRomSCCPlusSD()
{
	// unregister extra PSG I/O ports
	updateConfigReg(3);
	getCPUInterface().unregister_IO_Out_range(0x10, 2, this);
}

void MegaFlashRomSCCPlusSD::powerUp(EmuTime::param time)
{
	scc.powerUp(time);
	reset(time);
}

void MegaFlashRomSCCPlusSD::reset(EmuTime::param time)
{
	mapperReg = 0;
	offsetReg = 0;
	updateConfigReg(3);
	subslotReg = 0;
	ranges::iota(bankRegsSubSlot1, uint16_t(0));

	sccMode = 0;
	ranges::iota(sccBanks, byte(0));
	scc.reset(time);

	psgLatch = 0;
	psg.reset(time);

	flash.reset();

	// memory mapper
	for (auto [i, mr] : enumerate(memMapperRegs)) {
		mr = byte(3 - i);
	}

	for (auto [bank, reg] : enumerate(bankRegsSubSlot3)) {
		reg = (bank == 1) ? 1 : 0;
	}

	selectedCard = 0;

	invalidateDeviceRWCache(); // flush all to be sure
}

byte MegaFlashRomSCCPlusSD::getSubSlot(unsigned addr) const
{
	return isSlotExpanderEnabled() ?
		(subslotReg >> (2 * (addr >> 14))) & 3 : 1;
}

void MegaFlashRomSCCPlusSD::writeToFlash(unsigned addr, byte value)
{
	if (isFlashRomWriteEnabled()) {
		flash.write(addr, value);
	} else {
		// flash is write protected, this is implemented by not passing
		// writes to flash at all.
	}
}

byte MegaFlashRomSCCPlusSD::peekMem(word addr, EmuTime::param time) const
{
	if (isSlotExpanderEnabled() && (addr == 0xFFFF)) {
		// read subslot register
		return subslotReg ^ 0xFF;
	}

	switch (getSubSlot(addr)) {
		case 0: return peekMemSubSlot0(addr);
		case 1: return peekMemSubSlot1(addr, time);
		case 2: return isMemoryMapperEnabled() ?
				peekMemSubSlot2(addr) : 0xFF;
		case 3: return peekMemSubSlot3(addr, time);
		default: UNREACHABLE;
	}
}

byte MegaFlashRomSCCPlusSD::readMem(word addr, EmuTime::param time)
{
	if (isSlotExpanderEnabled() && (addr == 0xFFFF)) {
		// read subslot register
		return subslotReg ^ 0xFF;
	}

	switch (getSubSlot(addr)) {
		case 0: return readMemSubSlot0(addr);
		case 1: return readMemSubSlot1(addr, time);
		case 2: return isMemoryMapperEnabled() ?
				readMemSubSlot2(addr) : 0xFF;
		case 3: return readMemSubSlot3(addr, time);
		default: UNREACHABLE;
	}
}

const byte* MegaFlashRomSCCPlusSD::getReadCacheLine(word addr) const
{
	if (isSlotExpanderEnabled() &&
		((addr & CacheLine::HIGH) == (0xFFFF & CacheLine::HIGH))) {
		// read subslot register
		return nullptr;
	}

	switch (getSubSlot(addr)) {
		case 0: return getReadCacheLineSubSlot0(addr);
		case 1: return getReadCacheLineSubSlot1(addr);
		case 2: return isMemoryMapperEnabled() ?
				getReadCacheLineSubSlot2(addr) : unmappedRead.data();
		case 3: return getReadCacheLineSubSlot3(addr);
		default: UNREACHABLE;
	}
}

void MegaFlashRomSCCPlusSD::writeMem(word addr, byte value, EmuTime::param time)
{
	if (isSlotExpanderEnabled() && (addr == 0xFFFF)) {
		// write subslot register
		byte diff = value ^ subslotReg;
		subslotReg = value;
		for (auto i : xrange(4)) {
			if (diff & (3 << (2 * i))) {
				invalidateDeviceRWCache(0x4000 * i, 0x4000);
			}
		}
	}

	switch (getSubSlot(addr)) {
		case 0: writeMemSubSlot0(addr, value); break;
		case 1: writeMemSubSlot1(addr, value, time); break;
		case 2: if (isMemoryMapperEnabled()) {
				writeMemSubSlot2(addr, value);
			}
			break;
		case 3: writeMemSubSlot3(addr, value, time); break;
		default: UNREACHABLE;
	}
}

byte* MegaFlashRomSCCPlusSD::getWriteCacheLine(word addr)
{
	if (isSlotExpanderEnabled() &&
		((addr & CacheLine::HIGH) == (0xFFFF & CacheLine::HIGH))) {
		// read subslot register
		return nullptr;
	}

	switch (getSubSlot(addr)) {
		case 0: return getWriteCacheLineSubSlot0(addr);
		case 1: return getWriteCacheLineSubSlot1(addr);
		case 2: return isMemoryMapperEnabled() ?
				getWriteCacheLineSubSlot2(addr) : unmappedWrite.data();
		case 3: return getWriteCacheLineSubSlot3(addr);
		default: UNREACHABLE;
	}
}

/////////////////////// sub slot 0 ////////////////////////////////////////////

byte MegaFlashRomSCCPlusSD::readMemSubSlot0(word addr)
{
	// read from the first 16kB of flash
	// Pazos: ROM and flash can be accessed in all pages (0,1,2,3) (#0000-#FFFF)
	return flash.read(addr & 0x3FFF);
}

byte MegaFlashRomSCCPlusSD::peekMemSubSlot0(word addr) const
{
	// read from the first 16kB of flash
	// Pazos: ROM and flash can be accessed in all pages (0,1,2,3) (#0000-#FFFF)
	return flash.peek(addr & 0x3FFF);
}

const byte* MegaFlashRomSCCPlusSD::getReadCacheLineSubSlot0(word addr) const
{
	return flash.getReadCacheLine(addr & 0x3FFF);
}

void MegaFlashRomSCCPlusSD::writeMemSubSlot0(word addr, byte value)
{
	// Pazos: ROM and flash can be accessed in all pages (0,1,2,3) (#0000-#FFFF)
	writeToFlash(addr & 0x3FFF, value);
}

byte* MegaFlashRomSCCPlusSD::getWriteCacheLineSubSlot0(word /*addr*/)
{
	return nullptr; // flash isn't cacheable
}

/////////////////////// sub slot 1 ////////////////////////////////////////////

void MegaFlashRomSCCPlusSD::updateConfigReg(byte value)
{
	if ((value ^ configReg) & 0x08) {
		if (value & 0x08) {
			for (auto port : {0xa0, 0xa1}) {
				getCPUInterface().register_IO_Out(narrow_cast<byte>(port), this);
			}
		} else {
			for (auto port : {0xa0, 0xa1}) {
				getCPUInterface().unregister_IO_Out(narrow_cast<byte>(port), this);
			}
		}
	}
	configReg = value;
	flash.setVppWpPinLow(isFlashRomBlockProtectEnabled());
	invalidateDeviceRWCache(); // flush all to be sure
}

MegaFlashRomSCCPlusSD::SCCEnable MegaFlashRomSCCPlusSD::getSCCEnable() const
{
	if ((sccMode & 0x20) && (sccBanks[3] & 0x80)) {
		return EN_SCCPLUS;
	} else if ((!(sccMode & 0x20)) && ((sccBanks[2] & 0x3F) == 0x3F)) {
		return EN_SCC;
	} else {
		return EN_NONE;
	}
}

unsigned MegaFlashRomSCCPlusSD::getFlashAddrSubSlot1(unsigned addr) const
{
	unsigned page = is64KmapperConfigured() ? (addr >> 14) : ((addr >> 13) - 2);
	unsigned size = is64KmapperConfigured() ? 0x4000 : 0x2000;

	if (page >= 4) return unsigned(-1); // outside [0x4000, 0xBFFF] for non-64K mapper

	unsigned bank = bankRegsSubSlot1[page];
	if        (isDSKmodeEnabled() && (page == 0) && (bank == 0)) {
		bank = 0x3FA;
	} else if (isDSKmodeEnabled() && (page == 1) && (bank == 1)) {
		bank = 0x3FB;
	} else { // not DSK mode
		bank += offsetReg;
	}

	unsigned tmp = (bank * size) + (addr & (size - 1));
	return (tmp +  0x010000) & 0x7FFFFF; // wrap at 8MB
}

byte MegaFlashRomSCCPlusSD::readMemSubSlot1(word addr, EmuTime::param time)
{
	if (isKonamiSCCmapperConfigured()) { // Konami SCC
		SCCEnable enable = getSCCEnable();
		if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
		    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
			byte val = scc.readMem(narrow_cast<uint8_t>(addr & 0xFF), time);
			return val;
		}
	}

	unsigned flashAddr = getFlashAddrSubSlot1(addr);
	return (flashAddr != unsigned(-1))
		? flash.read(flashAddr)
		: 0xFF; // unmapped read
}

byte MegaFlashRomSCCPlusSD::peekMemSubSlot1(word addr, EmuTime::param time) const
{
	if (isKonamiSCCmapperConfigured()) { // Konami SCC
		SCCEnable enable = getSCCEnable();
		if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
		    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
			byte val = scc.peekMem(narrow_cast<uint8_t>(addr & 0xFF), time);
			return val;
		}
	}

	unsigned flashAddr = getFlashAddrSubSlot1(addr);
	return (flashAddr != unsigned(-1))
		? flash.peek(flashAddr)
		: 0xFF; // unmapped read
}

const byte* MegaFlashRomSCCPlusSD::getReadCacheLineSubSlot1(word addr) const
{
	if (isKonamiSCCmapperConfigured()) {
		SCCEnable enable = getSCCEnable();
		if (((enable == EN_SCC)     && (0x9800 <= addr) && (addr < 0xA000)) ||
		    ((enable == EN_SCCPLUS) && (0xB800 <= addr) && (addr < 0xC000))) {
			return nullptr;
		}
	}

	unsigned flashAddr = getFlashAddrSubSlot1(addr);
	return (flashAddr != unsigned(-1))
		? flash.getReadCacheLine(flashAddr)
		: unmappedRead.data();
}

void MegaFlashRomSCCPlusSD::writeMemSubSlot1(word addr, byte value, EmuTime::param time)
{
	// address is calculated before writes to other regions take effect
	unsigned flashAddr = getFlashAddrSubSlot1(addr);

	// There are several overlapping functional regions in the address
	// space. A single write can trigger behaviour in multiple regions. In
	// other words there's no priority amongst the regions where a higher
	// priority region blocks the write from the lower priority regions.
	// This only goes for places where the flash is 'seen', so not for the
	// SCC registers and the SSR

	if (!isConfigRegDisabled() && (addr == 0x7FFC)) {
		// write config register
		updateConfigReg(value);
	}

	if (!isMapperRegisterDisabled() && (addr == 0x7FFF)) {
		// write mapper register
		mapperReg = value;
		invalidateDeviceRWCache(); // flush all to be sure
	}

	if (!areBankRegsAndOffsetRegsDisabled() && (addr == 0x7FFD)) {
		// write offset register low part
		offsetReg = (offsetReg & 0x300) | value;
		invalidateDeviceRWCache();
	}

	if (!areBankRegsAndOffsetRegsDisabled() && (addr == 0x7FFE)) {
		// write offset register high part (bit 8 and 9)
		offsetReg = (offsetReg & 0xFF) + ((value & 0x3) << 8);
		invalidateDeviceRWCache();
	}

	if (isKonamiSCCmapperConfigured()) {
		// Konami-SCC
		if ((addr & 0xFFFE) == 0xBFFE) {
			sccMode = value;
			scc.setMode((value & 0x20) ? SCC::Mode::Plus
			                           : SCC::Mode::Compatible);
			invalidateDeviceRWCache(0x9800, 0x800);
			invalidateDeviceRWCache(0xB800, 0x800);
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

	unsigned page8kB = (addr >> 13) - 2;
	if (!areBankRegsAndOffsetRegsDisabled() && (page8kB < 4)) {
		// (possibly) write to bank registers
		switch (mapperReg & 0xE0) {
		case 0x00:
			// Konami-SCC
			if ((addr & 0x1800) == 0x1000) {
				// Storing 'sccBanks' may seem redundant at
				// first, but it's required to calculate
				// whether the SCC is enabled or not.
				sccBanks[page8kB] = value;
				// Masking of the mapper bits is done on
				// write (and only in Konami(-scc) mode)
				byte mask = areKonamiMapperLimitsEnabled() ? 0x3F : 0xFF;
				bankRegsSubSlot1[page8kB] = value & mask;
				invalidateDeviceRWCache(0x4000 + 0x2000 * page8kB, 0x2000);
			}
			break;
		case 0x20: {
			// Konami
			if (isWritingKonamiBankRegisterDisabled() && (addr < 0x6000)) {
				// Switching 0x4000-0x5FFF disabled.
				// This bit blocks writing to the bank register
				// (an alternative was forcing a 0 on read).
				// It only has effect in Konami mode.
				break;
			}
			// Making of the mapper bits is done on
			// write (and only in Konami(-scc) mode)
			if ((addr < 0x5000) || ((0x5800 <= addr) && (addr < 0x6000))) break; // only SCC range works
			byte mask = areKonamiMapperLimitsEnabled() ? 0x1F : 0xFF;
			bankRegsSubSlot1[page8kB] = value & mask;
			invalidateDeviceRWCache(0x4000 + 0x2000 * page8kB, 0x2000);
			break;
		}
		case 0x40:
		case 0x60:
			// 64kB
			bankRegsSubSlot1[page8kB] = value;
			invalidateDeviceRWCache(0x0000 + 0x4000 * page8kB, 0x4000);
			break;
		case 0x80:
		case 0xA0:
			// ASCII-8
			if ((0x6000 <= addr) && (addr < 0x8000)) {
				byte bank = (addr >> 11) & 0x03;
				bankRegsSubSlot1[bank] = value;
				invalidateDeviceRWCache(0x4000 + 0x2000 * bank, 0x2000);
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
			// In the first versions of the VHDL code, the bankRegs
			// were 8-bit, but due to the construction explained
			// above, this results in a maximum size of 2MB for the
			// ASCII-16 mapper, as we throw away 1 bit. Later,
			// Manuel Pazos made an update to use 9 bits for the
			// registers to overcome this limitation. We emulate
			// the updated version now.
			const uint16_t mask = (1 << 9) - 1;
			if ((0x6000 <= addr) && (addr < 0x6800)) {
				bankRegsSubSlot1[0] = (2 * value + 0) & mask;
				bankRegsSubSlot1[1] = (2 * value + 1) & mask;
				invalidateDeviceRWCache(0x4000, 0x4000);
			}
			if ((0x7000 <= addr) && (addr < 0x7800)) {
				bankRegsSubSlot1[2] = (2 * value + 0) & mask;
				bankRegsSubSlot1[3] = (2 * value + 1) & mask;
				invalidateDeviceRWCache(0x8000, 0x4000);
			}
			break;
		}
	}

	if (flashAddr != unsigned(-1)) {
		writeToFlash(flashAddr, value);
	}
}

byte* MegaFlashRomSCCPlusSD::getWriteCacheLineSubSlot1(word /*addr*/)
{
	return nullptr; // flash isn't cacheable
}

/////////////////////// sub slot 2 ////////////////////////////////////////////

unsigned MegaFlashRomSCCPlusSD::calcMemMapperAddress(word address) const
{
	auto bank = memMapperRegs[address >> 14];
	return ((bank & MEMORY_MAPPER_MASK) << 14) | (address & 0x3FFF);
}

byte MegaFlashRomSCCPlusSD::readMemSubSlot2(word addr)
{
	// read from the memory mapper
	return checkedRam->read(calcMemMapperAddress(addr));
}

byte MegaFlashRomSCCPlusSD::peekMemSubSlot2(word addr) const
{
	return checkedRam->peek(calcMemMapperAddress(addr));
}

const byte* MegaFlashRomSCCPlusSD::getReadCacheLineSubSlot2(word addr) const
{
	return checkedRam->getReadCacheLine(calcMemMapperAddress(addr));
}

void MegaFlashRomSCCPlusSD::writeMemSubSlot2(word addr, byte value)
{
	// write to the memory mapper
	checkedRam->write(calcMemMapperAddress(addr), value);
}

byte* MegaFlashRomSCCPlusSD::getWriteCacheLineSubSlot2(word addr)
{
	return checkedRam->getWriteCacheLine(calcMemMapperAddress(addr));
}

byte MegaFlashRomSCCPlusSD::MapperIO::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MegaFlashRomSCCPlusSD::MapperIO::peekIO(word port, EmuTime::param /*time*/) const
{
	return getSelectedSegment(port & 3) | byte(~MEMORY_MAPPER_MASK);
}

void MegaFlashRomSCCPlusSD::MapperIO::writeIO(word port, byte value, EmuTime::param /*time*/)
{
	mega.memMapperRegs[port & 3] = value & MEMORY_MAPPER_MASK;
	mega.invalidateDeviceRWCache(0x4000 * (port & 0x03), 0x4000);
}

byte MegaFlashRomSCCPlusSD::MapperIO::getSelectedSegment(byte page) const
{
	return mega.memMapperRegs[page];
}

/////////////////////// sub slot 3 ////////////////////////////////////////////

unsigned MegaFlashRomSCCPlusSD::getFlashAddrSubSlot3(unsigned addr) const
{
	unsigned page8kB = (addr >> 13) - 2;
	return (bankRegsSubSlot3[page8kB] & 0x7f) * 0x2000 + (addr & 0x1fff) + 0x700000;
}

byte MegaFlashRomSCCPlusSD::readMemSubSlot3(word addr, EmuTime::param /*time*/)
{
	if (((bankRegsSubSlot3[0] & 0xC0) == 0x40) && ((0x4000 <= addr) && (addr < 0x6000))) {
		// transfer from SD card
		return sdCard[selectedCard]->transfer(0xFF, (addr & 0x1000) != 0);
	}

	if ((0x4000 <= addr) && (addr < 0xC000)) {
		// read (flash)rom content
		unsigned flashAddr = getFlashAddrSubSlot3(addr);
		return flash.read(flashAddr);
	} else {
		// unmapped read
		return 0xFF;
	}
}

byte MegaFlashRomSCCPlusSD::peekMemSubSlot3(word addr, EmuTime::param /*time*/) const
{
	if ((0x4000 <= addr) && (addr < 0xC000)) {
		// read (flash)rom content
		unsigned flashAddr = getFlashAddrSubSlot3(addr);
		return flash.peek(flashAddr);
	} else {
		// unmapped read
		return 0xFF;
	}
}

const byte* MegaFlashRomSCCPlusSD::getReadCacheLineSubSlot3(word addr) const
{
	if (((bankRegsSubSlot3[0] & 0xC0) == 0x40) && ((0x4000 <= addr) && (addr < 0x6000))) {
		return nullptr;
	}

	if ((0x4000 <= addr) && (addr < 0xC000)) {
		// (flash)rom content
		unsigned flashAddr = getFlashAddrSubSlot3(addr);
		return flash.getReadCacheLine(flashAddr);
	} else {
		return unmappedRead.data();
	}
}

void MegaFlashRomSCCPlusSD::writeMemSubSlot3(word addr, byte value, EmuTime::param /*time*/)
{

	if (((bankRegsSubSlot3[0] & 0xC0) == 0x40) && ((0x4000 <= addr) && (addr < 0x6000))) {
		if (addr >= 0x5800) {
			selectedCard = value & 1;
		} else {
			// transfer to SD card
			sdCard[selectedCard]->transfer(value, (addr & 0x1000) != 0); // ignore return value
		}
	} else {
		// write to flash (first, before modifying bank regs)
		if ((0x4000 <= addr) && (addr < 0xC000)) {
			unsigned flashAddr = getFlashAddrSubSlot3(addr);
			writeToFlash(flashAddr, value);
		}

		// ASCII-8 mapper
		if ((0x6000 <= addr) && (addr < 0x8000)) {
			byte page8kB = (addr >> 11) & 0x03;
			bankRegsSubSlot3[page8kB] = value;
			invalidateDeviceRWCache(0x4000 + 0x2000 * page8kB, 0x2000);
		}
	}
}

byte* MegaFlashRomSCCPlusSD::getWriteCacheLineSubSlot3(word /*addr*/)
{
	return nullptr; // flash isn't cacheable
}

/////////////////////// I/O ////////////////////////////////////////////

void MegaFlashRomSCCPlusSD::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0xFF) {
		case 0xA0:
			if (!isPSGalsoMappedToNormalPorts()) return;
			[[fallthrough]];
		case 0x10:
			psgLatch = value & 0x0F;
			break;

		case 0xA1:
			if (!isPSGalsoMappedToNormalPorts()) return;
			[[fallthrough]];
		case 0x11:
			psg.writeRegister(psgLatch, value, time);
			break;

		default:
			UNREACHABLE;
	}
}

template<typename Archive>
void MegaFlashRomSCCPlusSD::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	// overall
	ar.serialize("flash",      flash,
	             "subslotReg", subslotReg);

	// subslot 0 stuff
	// (nothing)

	// subslot 1 stuff
	ar.serialize("scc",       scc,
	             "sccMode",   sccMode,
	             "sccBanks",  sccBanks,
	             "psg",       psg,
	             "psgLatch",  psgLatch,
	             "configReg", configReg,
	             "mapperReg", mapperReg,
	             "offsetReg", offsetReg,
	             "bankRegsSubSlot1", bankRegsSubSlot1);
	if constexpr (Archive::IS_LOADER) {
		// Re-register PSG ports (if needed)
		byte tmp = configReg;
		configReg = 3; // set to un-registered
		updateConfigReg(tmp); // restore correct value
	}

	// subslot 2 stuff
	// TODO ar.serialize("checkedRam", checkedRam);
	if (checkedRam) ar.serialize("ram", checkedRam->getUncheckedRam());
	ar.serialize("memMapperRegs", memMapperRegs);

	// subslot 3 stuff
	ar.serialize("bankRegsSubSlot3", bankRegsSubSlot3,
	             "selectedCard",     selectedCard,
	             "sdCard0",          *sdCard[0],
	             "sdCard1",          *sdCard[1]);
}
INSTANTIATE_SERIALIZE_METHODS(MegaFlashRomSCCPlusSD);
REGISTER_MSXDEVICE(MegaFlashRomSCCPlusSD, "MegaFlashRomSCCPlusSD");

} // namespace openmsx
