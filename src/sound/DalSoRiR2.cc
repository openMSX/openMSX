// TODO: for now copied from MSXMoonsound. Must be cleaned up.

// ATM this class does several things:
// - It connects the YMF278b chip to specific I/O ports in the MSX machine
// - It glues the YMF262 (FM-part) and YMF278 (Wave-part) classes together in a
//   full model of a YMF278b chip. IOW part of the logic of the YM278b is
//   modeled here instead of in a chip-specific class.
// TODO it would be nice to move the functionality of the 2nd point to a
//      different class, but until there's a 2nd user of this chip, this is
//      low priority.

#include "DalSoRiR2.hh"
#include "MSXCPUInterface.hh"
#include "Clock.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

// The master clock, running at 33.8MHz.
using MasterClock = Clock<33868800>;

// Required delay between register select and register read/write.
static constexpr auto FM_REG_SELECT_DELAY = MasterClock::duration(56);
// Required delay after register write.
static constexpr auto FM_REG_WRITE_DELAY  = MasterClock::duration(56);
// Datasheet doesn't mention any delay for reads from the FM registers. In fact
// it says reads from FM registers are not possible while tests on a real
// YMF278 show they do work (value of the NEW2 bit doesn't matter).

// Required delay between register select and register read/write.
static constexpr auto WAVE_REG_SELECT_DELAY = MasterClock::duration(88);
// Required delay after register write.
static constexpr auto WAVE_REG_WRITE_DELAY  = MasterClock::duration(88);
// Datasheet doesn't mention any delay for register reads (except for reads
// from register 6, see below). I also couldn't measure any delay on a real
// YMF278.

// Required delay after memory read.
static constexpr auto MEM_READ_DELAY  = MasterClock::duration(38);
// Required delay after memory write (instead of register write delay).
static constexpr auto MEM_WRITE_DELAY = MasterClock::duration(28);

// Required delay after instrument load.
// We pick 10000 cycles, this is approximately 300us (the number given in the
// datasheet). The exact number of cycles is unknown. But I did some (very
// rough) tests on real HW, and this number is not too bad (slightly too high
// but within 2%-4% of real value, needs more detailed tests).
static constexpr auto LOAD_DELAY = MasterClock::duration(10000);

static constexpr byte waveIOBase = 0x7E;

// DalSoRi2 specifics

// bit(s) of the memory bank selection registers regBank[n]
static constexpr byte REG_BANK_SEGMENT_MASK = (1 << 5) - 1; // 5-bits

// bit(s) of the frame selection registers regFrame[n]
static constexpr byte DISABLE = 1 << 7;
static constexpr byte REG_FRAME_SEGMENT_MASK = (1 << 3) - 1; // 3-bits

// bits of the configuration register regCfg
static constexpr byte ENA_S0 = 1 << 5;
static constexpr byte ENA_FW = 1 << 4;
static constexpr byte ENA_C4 = 1 << 1;
static constexpr byte ENA_C0 = 1 << 0;

DalSoRiR2::DalSoRiR2(const DeviceConfig& config)
	: MSXDevice(config)
	, ymf262(getName() + " FM", config, true)
	, ymf278(getName() + " wave",
	         4096, // 4MB RAM
	         YMF278::MemoryConfig::DalSoRiR2,
	         config)
	, ymf278LoadTime(getCurrentTime())
	, ymf278BusyTime(getCurrentTime())
	, sram(config, getName() + " SRAM", "DalSoRi R2 RAM", 0x8000)
	, flash(getName() + " flash", AmdFlashChip::SST39SF010, {}, config)
	, dipSwitchBDIS(getCommandController(),
		getName() + " DIP switch BDIS",
		"Controls the BDIS DIP switch position. ON = Disable BIOS (flash memory access)", false)
	, dipSwitchMCFG(getCommandController(),
		getName() + " DIP switch MCFG",
		"Controls the MCFG DIP switch position. ON = Enable sample SRAM instead of sample ROM on reset.", false)
	, dipSwitchIO_C0(getCommandController(),
		getName() + " DIP switch IO/C0",
		"Controls the IO/C0 DIP switch position. ON = Enable I/O addres C0H~C3H on reset. This is the Moonsound compatible I/O port range.", true)
	, dipSwitchIO_C4(getCommandController(),
		getName() + " DIP switch IO/C4",
		"Controls the IO/C4 DIP switch position. ON = Enable I/O addres C4H~C7H on reset. This is the MSX-AUDIO compatible I/O port range", true)
{
	for (auto p : xrange(2)) {
		getCPUInterface().register_IO_Out(waveIOBase + p, this);
		getCPUInterface().register_IO_In (waveIOBase + p, this);
	}

	powerUp(getCurrentTime());
}

DalSoRiR2::~DalSoRiR2()
{
	for (auto p : xrange(2)) {
		getCPUInterface().unregister_IO_Out(waveIOBase + p, this);
		getCPUInterface().unregister_IO_In (waveIOBase + p, this);
	}
	setRegCfg(0); // unregister any FM I/O ports
}

void DalSoRiR2::powerUp(EmuTime::param time)
{
	ymf278.clearRam();
	reset(time);
}

void DalSoRiR2::reset(EmuTime::param time)
{
	ymf262.reset(time);
	ymf278.reset(time);

	opl4latch = 0; // TODO check
	opl3latch = 0; // TODO check

	ymf278BusyTime = time;
	ymf278LoadTime = time;

	for (auto reg : xrange(4)) {
		regBank[reg] = reg;
	}
	for (auto reg : xrange(2)) {
		regFrame[reg] = reg;
	}
	setRegCfg((dipSwitchMCFG. getBoolean() ? ENA_S0 : 0) |
	          (dipSwitchIO_C0.getBoolean() ? ENA_C0 : 0) |
	          (dipSwitchIO_C4.getBoolean() ? ENA_C4 : 0));

	flash.reset();
}

void DalSoRiR2::setRegCfg(byte value)
{
	auto registerFMPortsFromBase = [this] (byte ioBase) {
		for (auto p : xrange(4)) {
			getCPUInterface().register_IO_Out(ioBase + p, this);
			getCPUInterface().register_IO_In (ioBase + p, this);
		}
	};
	auto unregisterFMPortsFromBase = [this] (byte ioBase) {
		for (auto p : xrange(4)) {
			getCPUInterface().unregister_IO_Out(ioBase + p, this);
			getCPUInterface().unregister_IO_In (ioBase + p, this);
		}
	};

	if ((value ^ regCfg) & ENA_C0) { // enable C0 changed
		if (value & ENA_C0) {
			registerFMPortsFromBase(0xC0);
		} else {
			unregisterFMPortsFromBase(0xC0);
		}
	}
	if ((value ^ regCfg) & ENA_C4) { // enable C4 changed
		if (value & ENA_C4) {
			registerFMPortsFromBase(0xC4);
		} else {
			unregisterFMPortsFromBase(0xC4);
		}
	}
	regCfg = value;
	ymf278.disableRomForDalSoRiR2((regCfg & ENA_S0) != 0);
	flash.setVppWpPinLow((regCfg & ENA_FW) == 0);
}

byte DalSoRiR2::readIO(word port, EmuTime::param time)
{
	if ((port & 0xFF) < 0xC0) {
		// WAVE part  0x7E-0x7F
		switch (port & 0x01) {
		case 0: // read latch, not supported
			return 255;
		case 1: // read wave register
			// Verified on real YMF278:
			// Even if NEW2=0 reads happen normally. Also reads
			// from sample memory (and thus the internal memory
			// pointer gets increased).
			if ((3 <= opl4latch) && (opl4latch <= 6)) {
				// This time is so small that on a MSX you can
				// never see BUSY=1. So I also couldn't test
				// whether this timing applies to registers 3-6
				// (like for write) or only to register 6. I
				// also couldn't test how the other registers
				// behave.
				// TODO Should we comment out this code? It
				// doesn't have any measurable effect on MSX.
				ymf278BusyTime = time + MEM_READ_DELAY;
			}
			return ymf278.readReg(opl4latch);
		default:
			UNREACHABLE;
		}
	} else {
		// FM part  0xC0-0xC7
		switch (port & 0x03) {
		case 0: // read status
		case 2:
			return ymf262.readStatus() | readYMF278Status(time);
		case 1:
		case 3: // read fm register
			return ymf262.readReg(opl3latch);
		default:
			UNREACHABLE;
		}
	}
}

byte DalSoRiR2::peekIO(word port, EmuTime::param time) const
{
	if ((port & 0xFF) < 0xC0) {
		// WAVE part  0x7E-0x7F
		switch (port & 0x01) {
		case 0: // read latch, not supported
			return 255;
		case 1: // read wave register
			return ymf278.peekReg(opl4latch);
		default:
			UNREACHABLE;
		}
	} else {
		// FM part  0xC0-0xC7
		switch (port & 0x03) {
		case 0: // read status
		case 2:
			return ymf262.peekStatus() | readYMF278Status(time);
		case 1:
		case 3: // read fm register
			return ymf262.peekReg(opl3latch);
		default:
			UNREACHABLE;
		}
	}
}

void DalSoRiR2::writeIO(word port, byte value, EmuTime::param time)
{
	if ((port & 0xFF) < 0xC0) {
		// WAVE part  0x7E-0x7F
		if (getNew2()) {
			switch (port & 0x01) {
			case 0: // select register
				ymf278BusyTime = time + WAVE_REG_SELECT_DELAY;
				opl4latch = value;
				break;
			case 1:
				if ((0x08 <= opl4latch) && (opl4latch <= 0x1F)) {
					ymf278LoadTime = time + LOAD_DELAY;
				}
				if ((3 <= opl4latch) && (opl4latch <= 6)) {
					// Note: this time is so small that on
					// MSX you never see BUSY=1 for these
					// registers. Confirmed on real HW that
					// also registers 3-5 are faster.
					ymf278BusyTime = time + MEM_WRITE_DELAY;
				} else {
					// For the other registers it is
					// possible to see BUSY=1, but only
					// very briefly and only on R800.
					ymf278BusyTime = time + WAVE_REG_WRITE_DELAY;
				}
				if (opl4latch == 0xf8) {
					ymf262.setMixLevel(value, time);
				} else if (opl4latch == 0xf9) {
					ymf278.setMixLevel(value, time);
				}
				ymf278.writeReg(opl4latch, value, time);
				break;
			default:
				UNREACHABLE;
			}
		} else {
			// Verified on real YMF278:
			// Writes are ignored when NEW2=0 (both register select
			// and register write).
		}
	} else {
		// FM part  0xC0-0xC7
		switch (port & 0x03) {
		case 0: // select register bank 0
			opl3latch = value;
			ymf278BusyTime = time + FM_REG_SELECT_DELAY;
			break;
		case 2: // select register bank 1
			opl3latch = value | 0x100;
			ymf278BusyTime = time + FM_REG_SELECT_DELAY;
			break;
		case 1:
		case 3: // write fm register
			ymf278BusyTime = time + FM_REG_WRITE_DELAY;
			ymf262.writeReg(opl3latch, value, time);
			break;
		default:
			UNREACHABLE;
		}
	}
}

byte DalSoRiR2::readMem(word addr, EmuTime::param /*time*/)
{
	if ((0x3000 <= addr) && (addr < 0x4000)) {
		// SRAM frame 0
		return sram[addr & 0x0FFF];
	} else if ((0x7000 <= addr) && (addr < 0x8000) && ((regFrame[0] & DISABLE) == 0)) {
		// SRAM frame 1
		return sram[(addr & 0x0FFF) + (regFrame[0] & REG_FRAME_SEGMENT_MASK) * 0x1000];
	} else if ((0xB000 <= addr) && (addr < 0xC000) && ((regFrame[1] & DISABLE) == 0)) {
		// SRAM frame 2
		return sram[(addr & 0x0FFF) + (regFrame[1] & REG_FRAME_SEGMENT_MASK) * 0x1000];
	} else if (!dipSwitchBDIS.getBoolean()) {
		// read selected segment of flash
		auto flashBank = addr >> 14;
		auto flashSegmentAddr = addr & 0x3FFF;
		return flash.read(flashSegmentAddr + (regBank[flashBank] & REG_BANK_SEGMENT_MASK) * 0x4000);
	}
	return 0xFF;
}

void DalSoRiR2::writeMem(word addr, byte value, EmuTime::param /*time*/)
{
	if ((0x3000 <= addr) && (addr < 0x4000)) {
		// SRAM frame 0
		sram[addr & 0x0FFF] = value;
	} else if ((0x7000 <= addr) && (addr < 0x8000) && ((regFrame[0] & DISABLE) == 0)) {
		// SRAM frame 1
		sram[(addr & 0x0FFF) + (regFrame[0] & REG_FRAME_SEGMENT_MASK) * 0x1000] = value;
	} else if ((0xB000 <= addr) && (addr < 0xC000) && ((regFrame[1] & DISABLE) == 0)) {
		// SRAM frame 2
		sram[(addr & 0x0FFF) + (regFrame[1] & REG_FRAME_SEGMENT_MASK) * 0x1000] = value;
	} else if ((0x6000 <= addr) && (addr < 0x6400)) {
		regBank [(addr >> 8) & 3] = value;
	} else if ((0x6500 <= addr) && (addr < 0x6700)) {
		regFrame[(addr >> 9) & 1] = value;
	} else if ((0x6700 <= addr) && (addr < 0x6800)) {
		setRegCfg(value);
	} else if (!dipSwitchBDIS.getBoolean()) {
		auto flashBank = addr >> 14;
		auto flashSegmentAddr = addr & 0x3FFF;
		flash.write(flashSegmentAddr + (regBank[flashBank] & REG_FRAME_SEGMENT_MASK) * 0x4000, value);
	}
}

bool DalSoRiR2::getNew2() const
{
	return (ymf262.peekReg(0x105) & 0x02) != 0;
}

byte DalSoRiR2::readYMF278Status(EmuTime::param time) const
{
	byte result = 0;
	if (time < ymf278BusyTime) result |= 0x01;
	if (time < ymf278LoadTime) result |= 0x02;
	return result;
}

// version 1: initial version
template<typename Archive>
void DalSoRiR2::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	auto backupRegCfg = regCfg;
	ar.serialize("ymf262",    ymf262,
	             "ymf278",    ymf278,
	             "opl3latch", opl3latch,
	             "opl4latch", opl4latch,
	             "loadTime",  ymf278LoadTime,
	             "sram",      sram,
	             "regBank",   regBank,
	             "regFrame",  regFrame,
	             "regCfg",    backupRegCfg);

	if constexpr (Archive::IS_LOADER) {
		setRegCfg(backupRegCfg);
	}
}
INSTANTIATE_SERIALIZE_METHODS(DalSoRiR2);
REGISTER_MSXDEVICE(DalSoRiR2, "DalSoRiR2");

} // namespace openmsx
