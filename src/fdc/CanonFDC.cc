#include "CanonFDC.hh"
#include "CacheLine.hh"
#include "serialize.hh"

// Technical info and discussion on:
//    https://www.msx.org/forum/msx-talk/hardware/canon-v-30f-msx2?page=8
//
// 0x7FF8: MB8877A command register (W) / Status register (R)
// 0x7FF9: MB8877A track register  (R/W)
// 0x7FFA: MB8877A sector register (R/W)
// 0x7FFB: MB8877A data register   (R/W)
// 0x7FFC: drive control
//   b1,b0: (R/W) drive select (0 = drive 0, 1 = drive 1, 3 = deselect drive)
//   b2:    (R/W) side select (0 = side 0, 1 = side 1)
//   b3:    (R/W) motor on (1 = motor on)
//   b4:     (R)  disk changed (0 = changed)
//   b5      (R)  some sort of ready (1 = ready)
//   b6      (R)  DRQ (1 = DRQ)
//   b7      (R)  IRQ (1 = IRQ)
// Hardware registers are also visible in page 2 (0xBFF8-0xBFFC).

namespace openmsx {

CanonFDC::CanonFDC(const DeviceConfig& config)
	: WD2793BasedFDC(config)
{
}

void CanonFDC::reset(EmuTime::param time)
{
	WD2793BasedFDC::reset(time);
	writeMem(0x3FFC, 0, time);
}

byte CanonFDC::readMem(word address, EmuTime::param time)
{
	switch (address & 0x3FFF) {
	case 0x3FF8:
		return controller.getStatusReg(time);
	case 0x3FF9:
		return controller.getTrackReg(time);
	case 0x3FFA:
		return controller.getSectorReg(time);
	case 0x3FFB:
		return controller.getDataReg(time);
	case 0x3FFC: {
		// At address 0x76F4 in the disk ROM bit 5 is checked. If this
		// bit is 0, the code executes a delay loop. Not sure what that
		// is for. Some kind of hardware debug feature??
		byte value = 0x30 | controlReg;
		if (controller.getIRQ(time))  value |= 0x40;
		if (controller.getDTRQ(time)) value |= 0x80;
		if (multiplexer.diskChanged()) value &= ~0x10;
		return value;
	}
	default:
		return CanonFDC::peekMem(address, time);
	}
}

byte CanonFDC::peekMem(word address, EmuTime::param time) const
{
	switch (address & 0x3FFF) {
	case 0x3FF8:
		return controller.peekStatusReg(time);
	case 0x3FF9:
		return controller.peekTrackReg(time);
	case 0x3FFA:
		return controller.peekSectorReg(time);
	case 0x3FFB:
		return controller.peekDataReg(time);
	case 0x3FFC: {
		byte value = 0x30 | controlReg;
		if (controller.peekIRQ(time))  value |= 0x40;
		if (controller.peekDTRQ(time)) value |= 0x80;
		if (multiplexer.peekDiskChanged()) value &= ~0x10;
		return value;
	}
	default:
		if (0x4000 <= address && address < 0x8000) {
			// ROM only visible in 0x4000-0x7FFF.   TODO double check.
			return MSXFDC::peekMem(address, time);
		} else {
			return 0xFF;
		}
	}
}

const byte* CanonFDC::getReadCacheLine(word start) const
{
	if ((start & 0x3FFF & CacheLine::HIGH) == (0x3FF0 & CacheLine::HIGH)) {
		// FDC at 0x7FF8-0x7FFC and 0xBFF8-0xBFFC
		return nullptr;
	} else if (0x4000 <= start && start < 0x8000) {
		// ROM at 0x4000-0x7FFF
		return MSXFDC::getReadCacheLine(start);
	} else {
		return unmappedRead;
	}
}

void CanonFDC::writeMem(word address, byte value, EmuTime::param time)
{
	switch (address & 0x3FFF) {
	case 0x3FF8:
		controller.setCommandReg(value, time);
		break;
	case 0x3FF9:
		controller.setTrackReg(value, time);
		break;
	case 0x3FFA:
		controller.setSectorReg(value, time);
		break;
	case 0x3FFB:
		controller.setDataReg(value, time);
		break;
	case 0x3FFC: {
		controlReg = value & 0x0F;
		auto drive = [&] {
			switch (value & 3) {
			case 0:  return DriveMultiplexer::DRIVE_A;
			case 1:  return DriveMultiplexer::DRIVE_B;
			default: return DriveMultiplexer::NO_DRIVE;
			}
		}();
		multiplexer.selectDrive(drive, time);
		multiplexer.setSide ((value & 0x04) != 0);
		multiplexer.setMotor((value & 0x08) != 0, time);
		break;
	}
	}
}

byte* CanonFDC::getWriteCacheLine(word address) const
{
	if ((address & 0x3FFF & CacheLine::HIGH) == (0x3FF0 & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return unmappedWrite;
	}
}

template<typename Archive>
void CanonFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
	ar.serialize("controlReg", controlReg);
}
INSTANTIATE_SERIALIZE_METHODS(CanonFDC);
REGISTER_MSXDEVICE(CanonFDC, "CanonFDC");

} // namespace openmsx
