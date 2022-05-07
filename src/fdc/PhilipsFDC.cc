#include "PhilipsFDC.hh"
#include "CacheLine.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "serialize.hh"

namespace openmsx {

PhilipsFDC::PhilipsFDC(const DeviceConfig& config)
	: WD2793BasedFDC(config)
{
	reset(getCurrentTime());
}

void PhilipsFDC::reset(EmuTime::param time)
{
	WD2793BasedFDC::reset(time);
	writeMem(0x3FFC, 0x00, time);
	writeMem(0x3FFD, 0x00, time);
}

byte PhilipsFDC::readMem(word address, EmuTime::param time)
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
	case 0x3FFD: {
		byte res = driveReg & ~4;
		if (!multiplexer.diskChanged()) {
			res |= 4;
		}
		return res;
	}
	case 0x3FFF: {
		byte value = 0xC0;
		if (controller.getIRQ(time)) value &= ~0x40;
		if (controller.getDTRQ(time)) value &= ~0x80;
		return value;
	}
	default:
		return PhilipsFDC::peekMem(address, time);
	}
}

byte PhilipsFDC::peekMem(word address, EmuTime::param time) const
{
	// FDC registers are mirrored in
	//   0x3FF8-0x3FFF
	//   0x7FF8-0x7FFF
	//   0xBFF8-0xBFFF
	//   0xFFF8-0xFFFF
	switch (address & 0x3FFF) {
	case 0x3FF8:
		return controller.peekStatusReg(time);
	case 0x3FF9:
		return controller.peekTrackReg(time);
	case 0x3FFA:
		return controller.peekSectorReg(time);
	case 0x3FFB:
		return controller.peekDataReg(time);
	case 0x3FFC:
		// bit 0 = side select
		// TODO check other bits !!
		return sideReg; // return multiplexer.getSideSelect();
	case 0x3FFD: {
		// bit 1,0 -> drive number
		// (00 or 10: drive A, 01: drive B, 11: nothing)
		// bit 2 -> 0 iff disk changed
		//      TODO This is required on Sony_HB-F500P.
		//           Do other machines have this bit as well?
		// bit 7 -> motor on
		// TODO check other bits !!
		byte res = driveReg & ~4;
		if (!multiplexer.peekDiskChanged()) {
			res |= 4;
		}
		return res;
	}
	case 0x3FFE:
		// not used
		return 255;
	case 0x3FFF: {
		// Drive control IRQ and DRQ lines are not connected to Z80
		// interrupt request
		// bit 6: !intrq
		// bit 7: !dtrq
		// TODO check other bits !!
		byte value = 0xC0;
		if (controller.peekIRQ(time)) value &= ~0x40;
		if (controller.peekDTRQ(time)) value &= ~0x80;
		return value;
	}
	default:
		if ((0x4000 <= address) && (address < 0x8000)) {
			// ROM only visible in 0x4000-0x7FFF
			return MSXFDC::peekMem(address, time);
		} else {
			return 255;
		}
	}
}

const byte* PhilipsFDC::getReadCacheLine(word start) const
{
	// if address overlap 0x7ff8-0x7ffb then return nullptr,
	// else normal ROM behaviour
	if ((start & 0x3FF8 & CacheLine::HIGH) == (0x3FF8 & CacheLine::HIGH)) {
		return nullptr;
	} else if ((0x4000 <= start) && (start < 0x8000)) {
		// ROM visible in 0x4000-0x7FFF
		return MSXFDC::getReadCacheLine(start);
	} else {
		return unmappedRead;
	}
}

void PhilipsFDC::writeMem(word address, byte value, EmuTime::param time)
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
	case 0x3FFC:
		// bit 0 = side select
		// TODO check other bits !!
		sideReg = value;
		multiplexer.setSide(value & 1);
		break;
	case 0x3FFD:
		// bit 1,0 -> drive number
		// (00 or 10: drive A, 01: drive B, 11: nothing)
		// TODO bit 6 -> drive LED (0 -> off, 1 -> on)
		// bit 7 -> motor on
		// TODO check other bits !!
		driveReg = value;
		DriveMultiplexer::DriveNum drive = [&] {
			switch (value & 3) {
				case 0:
				case 2:
					return DriveMultiplexer::DRIVE_A;
				case 1:
					return DriveMultiplexer::DRIVE_B;
				case 3:
				default:
					return DriveMultiplexer::NO_DRIVE;
			}
		}();
		multiplexer.selectDrive(drive, time);
		multiplexer.setMotor((value & 128) != 0, time);
		break;
	}
}

byte* PhilipsFDC::getWriteCacheLine(word address) const
{
	if ((address & 0x3FF8) == (0x3FF8 & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return unmappedWrite;
	}
}


void PhilipsFDC::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
	ar.serialize("sideReg",  sideReg,
	             "driveReg", driveReg);
}
INSTANTIATE_SERIALIZE_METHODS(PhilipsFDC);
REGISTER_MSXDEVICE(PhilipsFDC, "PhilipsFDC");

} // namespace openmsx
