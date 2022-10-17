/*
 * Based on code from NLMSX written by Frits Hilderink
 *
 * Despite the name, the class implements MSX connection to TC8566AF FDC for
 * any MSX that uses it. To make reuse possible, it implements two sets of
 * I/O configurations, one is used on turboR, the other on other machines with
 * this FDC.
 * There's also a mapper mechanism implemented. It's only used on the turboR,
 * but all other machines only have a 16kB diskROM, so it's practically
 * ineffective there. The mapper has 0x7FF0 as switch address. (Thanks to
 * NYYRIKKI for this info.)
 */

#include "TurboRFDC.hh"
#include "MSXCPU.hh"
#include "CacheLine.hh"
#include "Rom.hh"
#include "MSXException.hh"
#include "serialize.hh"

namespace openmsx {

[[nodiscard]] static TurboRFDC::Type parseType(const DeviceConfig& config)
{
	auto ioregs = config.getChildData("io_regs", {});
	if (ioregs == "7FF2") {
		return TurboRFDC::R7FF2;
	} else if (ioregs == "7FF8") {
		return TurboRFDC::R7FF8;
	} else if (ioregs.empty()) {
		// for backwards compatibility
		return TurboRFDC::BOTH;
	} else {
		throw MSXException(
			"Invalid 'io_regs' specification: expected one of "
			"'7FF2' or '7FF8', but got: ", ioregs);
	}
}

TurboRFDC::TurboRFDC(const DeviceConfig& config)
	: MSXFDC(config)
	, controller(getScheduler(), reinterpret_cast<DiskDrive**>(drives),
	             getCliComm(), getCurrentTime())
	, romBlockDebug(*this, std::span{&bank, 1}, 0x4000, 0x4000, 14)
	, blockMask((rom->size() / 0x4000) - 1)
	, type(parseType(config))
{
	reset(getCurrentTime());
}

void TurboRFDC::reset(EmuTime::param time)
{
	setBank(0);
	controller.reset(time);
}

byte TurboRFDC::readMem(word address, EmuTime::param time_)
{
	EmuTime time = time_;
	if (0x3FF0 <= (address & 0x3FFF)) {
		// Reading or writing to this region takes 1 extra clock
		// cycle. But only in R800 mode. Verified on a real turboR
		// machine, it happens for all 16 positions in this region
		// and both for reading and writing.
		time = getCPU().waitCyclesR800(time, 1);
		if (type != R7FF8) { // turboR or BOTH
			switch (address & 0xF) {
			case 0x1: {
				byte result = 0x33;
				if (controller.diskChanged(0)) result &= ~0x10;
				if (controller.diskChanged(1)) result &= ~0x20;
				return result;
			}
			case 0x4: return controller.readStatus(time);
			case 0x5: return controller.readDataPort(time);
			}
		}
		if (type != R7FF2) { // non-turboR or BOTH
			switch (address & 0xF) {
			case 0xA: return controller.readStatus(time);
			case 0xB: return controller.readDataPort(time);
			}

		}
	}
	// all other stuff is handled by peekMem()
	return TurboRFDC::peekMem(address, time);
}


byte TurboRFDC::peekMem(word address, EmuTime::param time) const
{
	if (0x3FF0 <= (address & 0x3FFF)) {
		// note: this implementation requires that the handled
		//    addresses for the MSX2 and TURBOR variants don't overlap
		if (type != R7FF8) { // turboR or BOTH
			switch (address & 0xF) {
			case 0x0: return bank;
			case 0x1: {
				// bit 0  FD2HD1  High Density detect drive 1
				// bit 1  FD2HD2  High Density detect drive 2
				// bit 4  FDCHG1  Disk Change detect on drive 1
				// bit 5  FDCHG2  Disk Change detect on drive 2
				// active low
				byte result = 0x33;
				if (controller.peekDiskChanged(0)) result &= ~0x10;
				if (controller.peekDiskChanged(1)) result &= ~0x20;
				return result;
			}
			case 0x4: return controller.peekStatus();
			case 0x5: return controller.peekDataPort(time);
			}
		}
		if (type != R7FF2) { // non-turboR or BOTH
			switch (address & 0xF) {
			case 0xA: return controller.peekStatus();
			case 0xB: return controller.peekDataPort(time);
			}
		}
		switch (address & 0xF) {
		// TODO Any idea what these 4 are? I've confirmed that on a
		// real FS-A1GT I get these values, though the ROM dumps
		// contain 0xFF in these locations. When looking at the ROM
		// content via the 'RomPanasonic' mapper in slot 3-3, you can
		// see that the ROM dumps are correct (these 4 values are not
		// part of the ROM).
		// This MRC post indicates that also on non-turbor machines
		// you see these 4 values (bluemsx' post of 10-03-2013, 10:15):
		//   http://www.msx.org/forum/msx-talk/development/finally-have-feature-request-openmsx?page=3
		case 0xC: return 0xFC;
		case 0xD: return 0xFC;
		case 0xE: return 0xFF;
		case 0xF: return 0x3F;
		default:  return 0xFF; // other regs in this region
		}
	} else if ((0x4000 <= address) && (address < 0x8000)) {
		return memory[address & 0x3FFF];
	} else {
		return 0xFF;
	}
}

const byte* TurboRFDC::getReadCacheLine(word start) const
{
	if ((start & 0x3FF0) == (0x3FF0 & CacheLine::HIGH)) {
		return nullptr;
	} else if ((0x4000 <= start) && (start < 0x8000)) {
		return &memory[start & 0x3FFF];
	} else {
		return unmappedRead;
	}
}

void TurboRFDC::writeMem(word address, byte value, EmuTime::param time_)
{
	EmuTime time = time_;
	if (0x3FF0 <= (address & 0x3FFF)) {
		// See comment in readMem().
		time = getCPU().waitCyclesR800(time, 1);
	}
	if (address == 0x7FF0) {
		setBank(value);
	} else {
		if (type != R7FF8) { // turboR or BOTH
			switch (address & 0x3FFF) {
			case 0x3FF2:
				controller.writeControlReg0(value, time);
				break;
			case 0x3ff3:
				controller.writeControlReg1(value, time);
				break;
			case 0x3FF5:
				controller.writeDataPort(value, time);
				break;
			}
		}
		if (type != R7FF2) { // non-turboR or BOTH
			switch (address & 0x3FFF) {
			case 0x3FF8:
				controller.writeControlReg0(value, time);
				break;
			case 0x3FF9:
				controller.writeControlReg1(value, time);
				break;
			case 0x3FFB:
				controller.writeDataPort(value, time);
				break;
			}
		}
	}
}

void TurboRFDC::setBank(byte value)
{
	invalidateDeviceRCache(0x4000, 0x4000);
	bank = value & blockMask;
	memory = &(*rom)[0x4000 * bank];
}

byte* TurboRFDC::getWriteCacheLine(word address) const
{
	if ((address == (0x7FF0 & CacheLine::HIGH)) ||
	    ((address & 0x3FF0) == (0x3FF0 & CacheLine::HIGH))) {
		return nullptr;
	} else {
		return unmappedWrite;
	}
}


template<typename Archive>
void TurboRFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXFDC>(*this);
	ar.serialize("TC8566AF", controller,
	             "bank",     bank);
	if constexpr (Archive::IS_LOADER) {
		setBank(bank);
	}
}
INSTANTIATE_SERIALIZE_METHODS(TurboRFDC);
REGISTER_MSXDEVICE(TurboRFDC, "TurboRFDC");

} // namespace openmsx
