#include "ReproCartridgeV1.hh"
#include "DummyAY8910Periphery.hh"
#include "MSXCPUInterface.hh"
#include "narrow.hh"
#include "serialize.hh"
#include <array>


/******************************************************************************
 * DOCUMENTATION AS PROVIDED BY MANUEL PAZOS, WHO DEVELOPED THE CARTRIDGE     *
 ******************************************************************************

   Repro Cartridge version 1 is similar to Konami Ultimate Collection. It
   uses the same flashROM, SCC/SCC+. But it only supports Konami SCC mapper.

   Released were cartridges with the following content (at least): only Metal
   Gear, only Metal Gear 2

[REGISTER (#7FFF)]
If it contains value 0x50, the flash is writable and the mapper is disabled.
Otherwise, the mapper is enabled and the flash is readonly.

- Mapper supports 4 different ROMs of 2MB each, with the KonamiSCC mapper
- Cartridge has a PSG at 0x10, write only
- On I/O port 0x13 the 2MB block can be selected (default 0, so up to 3)

******************************************************************************/

namespace openmsx {

ReproCartridgeV1::ReproCartridgeV1(
		DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, flash(rom, AmdFlashChip::M29W640GB, {}, config)
	, scc("ReproCartV1 SCC", config, getCurrentTime(), SCC::Mode::Compatible)
	, psg("ReproCartV1 PSG", DummyAY8910Periphery::instance(), config,
	      getCurrentTime())
{
	// adjust PSG volume, see details in https://github.com/openMSX/openMSX/issues/1934
	// note: this is a theoretical value. The actual relative volume should be measured!
	psg.setSoftwareVolume(21000.0f/9000.0f, getCurrentTime());

	powerUp(getCurrentTime());
	auto& cpuInterface = getCPUInterface();
	for (auto port : {0x10, 0x11, 0x13}) {
		cpuInterface.register_IO_Out(narrow_cast<byte>(port), this);
	}
}

ReproCartridgeV1::~ReproCartridgeV1()
{
	auto& cpuInterface = getCPUInterface();
	for (auto port : {0x10, 0x11, 0x13}) {
		cpuInterface.unregister_IO_Out(narrow_cast<byte>(port), this);
	}
}

void ReproCartridgeV1::powerUp(EmuTime time)
{
	scc.powerUp(time);
	reset(time);
}

void ReproCartridgeV1::reset(EmuTime time)
{
	flashRomWriteEnabled = false;
	mainBankReg = 0;
	sccMode = 0;
	ranges::iota(bankRegs, byte(0));

	scc.reset(time);
	psgLatch = 0;
	psg.reset(time);

	flash.reset();

	invalidateDeviceRCache(); // flush all to be sure
}

unsigned ReproCartridgeV1::getFlashAddr(unsigned addr) const
{
	unsigned page8kB = (addr >> 13) - 2;
	if (page8kB >= 4) return unsigned(-1); // outside [0x4000, 0xBFFF]

	byte bank = bankRegs[page8kB]; // 2MB max
	return (mainBankReg << 21) | (bank << 13) | (addr & 0x1FFF);
}

// Note: implementation (mostly) copied from KUC
bool ReproCartridgeV1::isSCCAccess(uint16_t addr) const
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

byte ReproCartridgeV1::readMem(uint16_t addr, EmuTime time)
{
	if (isSCCAccess(addr)) {
		return scc.readMem(narrow_cast<uint8_t>(addr & 0xFF), time);
	}

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.read(flashAddr, time)
		: 0xFF; // unmapped read
}

byte ReproCartridgeV1::peekMem(uint16_t addr, EmuTime time) const
{
	if (isSCCAccess(addr)) {
		return scc.peekMem(narrow_cast<uint8_t>(addr & 0xFF), time);
	}

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.peek(flashAddr, time)
		: 0xFF; // unmapped read
}

const byte* ReproCartridgeV1::getReadCacheLine(uint16_t addr) const
{
	if (isSCCAccess(addr)) return nullptr;

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.getReadCacheLine(flashAddr)
		: unmappedRead.data();
}

void ReproCartridgeV1::writeMem(uint16_t addr, byte value, EmuTime time)
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

	if (!flashRomWriteEnabled) {
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
	} else {
		if (flashAddr != unsigned(-1)) {
			flash.write(flashAddr, value, time);
		}
	}
}

byte* ReproCartridgeV1::getWriteCacheLine(uint16_t addr)
{
	return ((0x4000 <= addr) && (addr < 0xC000))
	       ? nullptr        // [0x4000,0xBFFF] isn't cacheable
	       : unmappedWrite.data();
}

void ReproCartridgeV1::writeIO(uint16_t port, byte value, EmuTime time)
{
	switch (port & 0xFF)
	{
		case 0x10:
			psgLatch = value & 0x0F;
			break;
		case 0x11:
			psg.writeRegister(psgLatch, value, time);
			break;
		case 0x13:
			mainBankReg = value & 3;
			invalidateDeviceRCache(); // flush all to be sure
			break;
		default: UNREACHABLE;
	}
}

template<typename Archive>
void ReproCartridgeV1::serialize(Archive& ar, unsigned /*version*/)
{
	// skip MSXRom base class
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("flash",       flash,
	             "scc",         scc,
	             "psg",         psg,
	             "psgLatch",    psgLatch,
	             "flashRomWriteEnabled", flashRomWriteEnabled,
	             "mainBankReg", mainBankReg,
	             "sccMode",     sccMode,
	             "bankRegs",    bankRegs);
}
INSTANTIATE_SERIALIZE_METHODS(ReproCartridgeV1);
REGISTER_MSXDEVICE(ReproCartridgeV1, "ReproCartridgeV1");

} // namespace openmsx
