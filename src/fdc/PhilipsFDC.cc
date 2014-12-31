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
	byte value;
	switch (address & 0x3FFF) {
	case 0x3FF8:
		value = controller.getStatusReg(time);
		break;
	case 0x3FF9:
		value = controller.getTrackReg(time);
		break;
	case 0x3FFA:
		value = controller.getSectorReg(time);
		break;
	case 0x3FFB:
		value = controller.getDataReg(time);
		break;
	case 0x3FFF:
		value = 0xC0;
		if (controller.getIRQ(time)) value &= ~0x40;
		if (controller.getDTRQ(time)) value &= ~0x80;
		break;
	default:
		value = PhilipsFDC::peekMem(address, time);
		break;
	}
	return value;
}

byte PhilipsFDC::peekMem(word address, EmuTime::param time) const
{
	byte value;
	// FDC registers are mirrored in
	//   0x3FF8-0x3FFF
	//   0x7FF8-0x7FFF
	//   0xBFF8-0xBFFF
	//   0xFFF8-0xFFFF
	switch (address & 0x3FFF) {
	case 0x3FF8:
		value = controller.peekStatusReg(time);
		break;
	case 0x3FF9:
		value = controller.peekTrackReg(time);
		break;
	case 0x3FFA:
		value = controller.peekSectorReg(time);
		break;
	case 0x3FFB:
		value = controller.peekDataReg(time);
		break;
	case 0x3FFC:
		// bit 0 = side select
		// TODO check other bits !!
		value = sideReg; // value = multiplexer.getSideSelect();
		break;
	case 0x3FFD:
		// bit 1,0 -> drive number
		// (00 or 10: drive A, 01: drive B, 11: nothing)
		// bit 7 -> motor on
		// TODO check other bits !!
		value = driveReg; // multiplexer.getSelectedDrive();
		break;
	case 0x3FFE:
		// not used
		value = 255;
		break;
	case 0x3FFF:
		// Drive control IRQ and DRQ lines are not connected to Z80
		// interrupt request
		// bit 6: !intrq
		// bit 7: !dtrq
		// TODO check other bits !!
		value = 0xC0;
		if (controller.peekIRQ(time)) value &= ~0x40;
		if (controller.peekDTRQ(time)) value &= ~0x80;
		break;

	default:
		if ((0x4000 <= address) && (address < 0x8000)) {
			// ROM only visible in 0x4000-0x7FFF
			value = MSXFDC::peekMem(address, time);
		} else {
			value = 255;
		}
		break;
	}
	return value;
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
		DriveMultiplexer::DriveNum drive;
		switch (value & 3) {
			case 0:
			case 2:
				drive = DriveMultiplexer::DRIVE_A;
				break;
			case 1:
				drive = DriveMultiplexer::DRIVE_B;
				break;
			case 3:
			default:
				drive = DriveMultiplexer::NO_DRIVE;
		}
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


template<typename Archive>
void PhilipsFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
	ar.serialize("sideReg", sideReg);
	ar.serialize("driveReg", driveReg);
}
INSTANTIATE_SERIALIZE_METHODS(PhilipsFDC);
REGISTER_MSXDEVICE(PhilipsFDC, "PhilipsFDC");

} // namespace openmsx
