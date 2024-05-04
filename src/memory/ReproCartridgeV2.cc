#include "ReproCartridgeV2.hh"

#include "DummyAY8910Periphery.hh"
#include "MSXCPUInterface.hh"
#include "serialize.hh"

#include "narrow.hh"

#include <array>


/******************************************************************************
 * DOCUMENTATION AS PROVIDED BY MANUEL PAZOS, WHO DEVELOPED THE CARTRIDGE     *
 ******************************************************************************

   Repro Cartridge version 2 is similar to Konami Ultimate Collection. It
   uses the same flashROM, SCC/SCC+. But 2 PSG's and volume control and
   multiple mappers.

   Released were cartridge with the following content: Only Metal Gear, only
   Metal Gear 2, Vampire Killer (original, Castlevania patch, enhanced SCC),
   Usas (original, enhanced), Nemesis 3 (enhanced) and Metal Gear Collection
   (Metal Gear 1 and 2, both in 3 languages)


[REGISTER (#7FFE)]
Selects the mapper, using bits 0 and 1 (other bits are ignored):
    00 = Konami SCC
    01 = Konami
    10 = ASCII8
    11 = ASCII16

[REGISTER (#7FFF)]
If it contains value 0x50, the flash is writable and the mapper is disabled.
Otherwise, the mapper is enabled and the flash is readonly.

- Mapper supports 8 different ROMs of 1MB each, with the above mappers.
- Cartridge has a PSG at 0x10 and a PSG at 0xA0, both write only
- On I/O port 0x33 the 1MB block can be selected (default 0, so up to 7)
- On I/O port 0x31 the volume can be selected of the 3 sound chips:
  Bits EEIIISSS:  EE(7-6) = PSG#10, III(5-3) = PSG#A0, SSS(2-0) = SCC
  Default value is &B10000010. 0 means silent. So, clone PSG is silent by
  default.

Note: there is also a version 1 of this hardware, with the following
differences:
- Only Konami SCC mapper (no register at #7FFE)
- No volume control register behind port #31
- Main bank register is behind port #13 instead of #33
- Main block size is 2MB instead of 1MB
- No extra PSG at 0xA0 (but the PSG at #10 is there)

******************************************************************************/

namespace openmsx {

ReproCartridgeV2::ReproCartridgeV2(
		const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, flash(rom, AmdFlashChip::M29W640GB, {}, config)
	, scc("ReproCartV2 SCC", config, getCurrentTime(), SCC::Mode::Compatible)
	, psg0x10("ReproCartV2 PSG@0x10", DummyAY8910Periphery::instance(), config,
	      getCurrentTime())
	, psg0xA0("ReproCartV2 PSG@0xA0", DummyAY8910Periphery::instance(), config,
	      getCurrentTime())
{
	powerUp(getCurrentTime());
	auto& cpuInterface = getCPUInterface();
	for (auto port : {0x10, 0x11, 0x31, 0x33, 0xA0, 0xA1}) {
		cpuInterface.register_IO_Out(narrow_cast<byte>(port), this);
	}
}

ReproCartridgeV2::~ReproCartridgeV2()
{
	auto& cpuInterface = getCPUInterface();
	for (auto port : {0x10, 0x11, 0x31, 0x33, 0xA0, 0xA1}) {
		cpuInterface.unregister_IO_Out(narrow_cast<byte>(port), this);
	}
}

void ReproCartridgeV2::powerUp(EmuTime::param time)
{
	scc.powerUp(time);
	reset(time);
}

void ReproCartridgeV2::reset(EmuTime::param time)
{
	flashRomWriteEnabled = false;
	mainBankReg = 0;
	setVolume(time, 0b10'000'10);
	mapperTypeReg = 0;
	sccMode = 0;
	ranges::iota(bankRegs, byte(0));

	scc.reset(time);
	psg0x10Latch = 0;
	psg0x10.reset(time);
	psg0xA0Latch = 0;
	psg0xA0.reset(time);

	flash.reset();

	invalidateDeviceRCache(); // flush all to be sure
}

unsigned ReproCartridgeV2::getFlashAddr(unsigned addr) const
{
	unsigned page8kB = (addr >> 13) - 2;
	if (page8kB >= 4) return unsigned(-1); // outside [0x4000, 0xBFFF]

	byte bank = bankRegs[page8kB] & 127; // 1MB max
	return (mainBankReg << 20) | (bank << 13) | (addr & 0x1FFF);
}

// Note: implementation (mostly) copied from KUC
bool ReproCartridgeV2::isSCCAccess(word addr) const
{
	if ((mapperTypeReg != 0) || (sccMode & 0x10)) return false;

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

byte ReproCartridgeV2::readMem(word addr, EmuTime::param time)
{
	if (isSCCAccess(addr)) {
		return scc.readMem(narrow_cast<uint8_t>(addr & 0xFF), time);
	}

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.read(flashAddr)
		: 0xFF; // unmapped read
}

byte ReproCartridgeV2::peekMem(word addr, EmuTime::param time) const
{
	if (isSCCAccess(addr)) {
		return scc.peekMem(narrow_cast<uint8_t>(addr & 0xFF), time);
	}

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.peek(flashAddr)
		: 0xFF; // unmapped read
}

const byte* ReproCartridgeV2::getReadCacheLine(word addr) const
{
	if (isSCCAccess(addr)) return nullptr;

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.getReadCacheLine(flashAddr)
		: unmappedRead.data();
}

void ReproCartridgeV2::writeMem(word addr, byte value, EmuTime::param time)
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

	// Main mapper register
	if (addr == 0x7FFF) {
		flashRomWriteEnabled = (value == 0x50);
		invalidateDeviceRCache(); // flush all to be sure
	}
	// Mapper selection register
	if (addr == 0x7FFE) {
		mapperTypeReg = value & 3; // other bits are ignored, so no need to store
		invalidateDeviceRCache(); // flush all to be sure
	}

	if (!flashRomWriteEnabled) {
		switch (mapperTypeReg) {
		case 0:
			// Konami-SCC
			if ((addr & 0x1800) == 0x1000) {
				// [0x5000,0x57FF] [0x7000,0x77FF]
				// [0x9000,0x97FF] [0xB000,0xB7FF]
				bankRegs[page8kB] = value;
				invalidateDeviceRCache(0x4000 + 0x2000 * page8kB, 0x2000);
			}

			// SCC mode register
			if ((addr & 0xFFFE) == 0xBFFE) {
				sccMode = value;
				scc.setMode((value & 0x20) ? SCC::Mode::Plus
				                           : SCC::Mode::Compatible);
				invalidateDeviceRCache(0x9800, 0x800);
				invalidateDeviceRCache(0xB800, 0x800);
			}
			break;
		case 1:
		{
			// Konami
			// (Copied from MegaFlashROMSCCPlus)
			// Masking of the mapper bits is done on
			// write (and only in Konami(-scc) mode)
			if ((addr < 0x5000) || ((0x5800 <= addr) && (addr < 0x6000))) break; // only SCC range works
			bankRegs[page8kB] = value & 0x7F;
			invalidateDeviceRCache(0x4000 + 0x2000 * page8kB, 0x2000);
			break;
		}
		case 2:
			// ASCII-8
			// (Copied from MegaFlashROMSCCPlus)
			if ((0x6000 <= addr) && (addr < 0x8000)) {
				byte bank = (addr >> 11) & 0x03;
				bankRegs[bank] = value;
				invalidateDeviceRCache(0x4000 + 0x2000 * bank, 0x2000);
			}
			break;
		case 3:
			// ASCII-16
			// (Copied from MegaFlashROMSCCPlus)
			// This behaviour is confirmed by Manuel Pazos (creator
			// of the cartridge): ASCII-16 uses all 4 bank registers
			// and one bank switch changes 2 registers at once.
			// This matters when switching mapper mode, because
			// the content of the bank registers is unchanged after
			// a switch.
			if ((0x6000 <= addr) && (addr < 0x6800)) {
				bankRegs[0] = narrow_cast<byte>(2 * value + 0);
				bankRegs[1] = narrow_cast<byte>(2 * value + 1);
				invalidateDeviceRCache(0x4000, 0x4000);
			}
			if ((0x7000 <= addr) && (addr < 0x7800)) {
				bankRegs[2] = narrow_cast<byte>(2 * value + 0);
				bankRegs[3] = narrow_cast<byte>(2 * value + 1);
				invalidateDeviceRCache(0x8000, 0x4000);
			}
			break;
		default:
			UNREACHABLE;
		}
	} else {
		if (flashAddr != unsigned(-1)) {
			flash.write(flashAddr, value);
		}
	}
}

byte* ReproCartridgeV2::getWriteCacheLine(word addr)
{
	return ((0x4000 <= addr) && (addr < 0xC000))
	       ? nullptr        // [0x4000,0xBFFF] isn't cacheable
	       : unmappedWrite.data();
}

void ReproCartridgeV2::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0xFF)
	{
		case 0x10:
			psg0x10Latch = value & 0x0F;
			break;
		case 0x11:
			psg0x10.writeRegister(psg0x10Latch, value, time);
			break;
		case 0xA0:
			psg0xA0Latch = value & 0x0F;
			break;
		case 0xA1:
			psg0xA0.writeRegister(psg0xA0Latch, value, time);
			break;
		case 0x31:
			setVolume(time, value);
			break;
		case 0x33:
			mainBankReg = value & 7;
			invalidateDeviceRCache(); // flush all to be sure
			break;
		default: UNREACHABLE;
	}
}

void ReproCartridgeV2::setVolume(EmuTime::param time, byte value)
{
	// store (mostly for the save/loadstate feature)
	volumeReg = value;
	// EE (7-6) = PSG#10. So values 0-3
	// III(5-3) = PSG#A0. So values 0-7
	// SSS(2-0) = SCC.    So values 0-7
	scc    .setSoftwareVolume(narrow<float>((volumeReg >> 0) & 7) * 0.5f, time);
	psg0xA0.setSoftwareVolume(narrow<float>((volumeReg >> 3) & 7) * 0.5f, time);
	psg0x10.setSoftwareVolume(narrow<float>((volumeReg >> 6) & 3) * 0.5f, time);
}

template<typename Archive>
void ReproCartridgeV2::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("flash",         flash,
	             "scc",           scc,
	             "psg0x10",       psg0x10,
	             "psg0x10Latch",  psg0x10Latch,
	             "psg0xA0",       psg0xA0,
	             "psg0xA0Latch",  psg0xA0Latch,
	             "flashRomWriteEnabled", flashRomWriteEnabled,
	             "mainBankReg",   mainBankReg,
	             "volumeReg",     volumeReg,
	             "mapperTypeReg", mapperTypeReg,
	             "sccMode",       sccMode,
	             "bankRegs",      bankRegs);

	if constexpr (Archive::IS_LOADER) {
		auto time = getCurrentTime();
		setVolume(time, volumeReg);
	}

}
INSTANTIATE_SERIALIZE_METHODS(ReproCartridgeV2);
REGISTER_MSXDEVICE(ReproCartridgeV2, "ReproCartridgeV2");

} // namespace openmsx
