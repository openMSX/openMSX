#include "ReproCartridgeV1.hh"
#include "DummyAY8910Periphery.hh"
#include "MSXCPUInterface.hh"
#include "cstd.hh"
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

static constexpr auto sectorInfo = [] {
	// 8 * 8kB, followed by 127 * 64kB
	using Info = AmdFlash::SectorInfo;
	std::array<Info, 8 + 127> result = {};
	cstd::fill(result.begin(), result.begin() + 8, Info{ 8 * 1024, false});
	cstd::fill(result.begin() + 8, result.end(),   Info{64 * 1024, false});
	return result;
}();

ReproCartridgeV1::ReproCartridgeV1(
		const DeviceConfig& config, Rom&& rom_)
	: MSXRom(config, std::move(rom_))
	, flash(rom, sectorInfo, 0x207E,
	        AmdFlash::Addressing::BITS_12, config)
	, scc("MGCV1 SCC", config, getCurrentTime(), SCC::SCC_Compatible)
	, psg("MGCV1 PSG", DummyAY8910Periphery::instance(), config,
	      getCurrentTime())
{
	powerUp(getCurrentTime());

	getCPUInterface().register_IO_Out(0x10, this);
	getCPUInterface().register_IO_Out(0x11, this);
	getCPUInterface().register_IO_Out(0x13, this);
}

ReproCartridgeV1::~ReproCartridgeV1()
{
	getCPUInterface().unregister_IO_Out(0x10, this);
	getCPUInterface().unregister_IO_Out(0x11, this);
	getCPUInterface().unregister_IO_Out(0x13, this);
}

void ReproCartridgeV1::powerUp(EmuTime::param time)
{
	scc.powerUp(time);
	reset(time);
}

void ReproCartridgeV1::reset(EmuTime::param time)
{
	flashRomWriteEnabled = false;
	mainBankReg = 0;
	sccMode = 0;
	ranges::iota(bankRegs, 0);

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
bool ReproCartridgeV1::isSCCAccess(word addr) const
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

byte ReproCartridgeV1::readMem(word addr, EmuTime::param time)
{
	if (isSCCAccess(addr)) {
		return scc.readMem(addr & 0xFF, time);
	}

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.read(flashAddr)
		: 0xFF; // unmapped read
}

byte ReproCartridgeV1::peekMem(word addr, EmuTime::param time) const
{
	if (isSCCAccess(addr)) {
		return scc.peekMem(addr & 0xFF, time);
	}

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.peek(flashAddr)
		: 0xFF; // unmapped read
}

const byte* ReproCartridgeV1::getReadCacheLine(word addr) const
{
	if (isSCCAccess(addr)) return nullptr;

	unsigned flashAddr = getFlashAddr(addr);
	return (flashAddr != unsigned(-1))
		? flash.getReadCacheLine(flashAddr)
		: unmappedRead;
}

void ReproCartridgeV1::writeMem(word addr, byte value, EmuTime::param time)
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
		scc.writeMem(addr & 0xFF, value, time);
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
			scc.setChipMode((value & 0x20) ? SCC::SCC_plusmode
						       : SCC::SCC_Compatible);
			invalidateDeviceRCache(0x9800, 0x800);
			invalidateDeviceRCache(0xB800, 0x800);
		}
	} else {
		if (flashAddr != unsigned(-1)) {
			flash.write(flashAddr, value);
		}
	}
}

byte* ReproCartridgeV1::getWriteCacheLine(word addr) const
{
	return ((0x4000 <= addr) && (addr < 0xC000))
	       ? nullptr        // [0x4000,0xBFFF] isn't cacheable
	       : unmappedWrite;
}

void ReproCartridgeV1::writeIO(word port, byte value, EmuTime::param time)
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
