#include "VictorFDC.hh"
#include "CacheLine.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "serialize.hh"

// This implementation is documented in the HC-95 service manual:
//
// FDD interface:
// 7FF8   I/O            FDC STATUS/COMMAND
// 7FF9   I/O            FDC TRACK REGISTER
// 7FFA   I/O            FDC SECTOR REGISTER
// 7FFB   I/O            FDC DATA REGISTER
// 7FFC   I/O   bit 0    A DRIVE MOTOR ON/OFF        "1" ON
//        I/O   bit 1    B DRIVE MOTOR ON/OFF        "1" ON
//        I/O   bit 2    DRIVE SELECT "0" A DRIVE    "1" B DRIVE
//        O     bit 3    SIDE SELECT "0" SIDE 0      "1" SIDE 1
//        I              SIDE SELECT "1" SIDE 0      "0" SIDE 1
//        I/O   bit 4    DRIVE ENABLE                "0" ENABLE
//              bit 5    unused
//        I     bit 6    FDC DATA REQUEST            "1" REQUEST
//        I     bit 7    FDC INTERRUPT REQUEST       "1" REQUEST

namespace openmsx {

constexpr int DRIVE_A_MOTOR = 0x01;
constexpr int DRIVE_B_MOTOR = 0x02;
constexpr int DRIVE_SELECT  = 0x04;
constexpr int SIDE_SELECT   = 0x08;
constexpr int DRIVE_DISABLE = 0x10; // renamed due to inverse logic
constexpr int DATA_REQUEST  = 0x40;
constexpr int INTR_REQUEST  = 0x80;


VictorFDC::VictorFDC(const DeviceConfig& config)
	: WD2793BasedFDC(config)
{
	reset(getCurrentTime());
}

void VictorFDC::reset(EmuTime::param time)
{
	WD2793BasedFDC::reset(time);
	// initialize in such way that drives are disabled
	// (and motors off, etc.)
	// TODO: test on real machine (this is an assumption)
	writeMem(0x7FFC, DRIVE_DISABLE, time);
}

byte VictorFDC::readMem(word address, EmuTime::param time)
{
	switch (address) {
	case 0x7FF8:
		return controller.getStatusReg(time);
	case 0x7FF9:
		return controller.getTrackReg(time);
	case 0x7FFA:
		return controller.getSectorReg(time);
	case 0x7FFB:
		return controller.getDataReg(time);
	case 0x7FFC: {
		byte value = driveControls;
		if (controller.getIRQ(time))  value |= INTR_REQUEST;
		if (controller.getDTRQ(time)) value |= DATA_REQUEST;
		value ^= SIDE_SELECT; // inverted
		return value;
	}
	default:
		return VictorFDC::peekMem(address, time);
	}
}

byte VictorFDC::peekMem(word address, EmuTime::param time) const
{
	switch (address) {
	case 0x7FF8:
		return controller.peekStatusReg(time);
	case 0x7FF9:
		return controller.peekTrackReg(time);
	case 0x7FFA:
		return controller.peekSectorReg(time);
	case 0x7FFB:
		return controller.peekDataReg(time);
	case 0x7FFC: {
		byte value = driveControls;
		if (controller.peekIRQ(time))  value |= INTR_REQUEST;
		if (controller.peekDTRQ(time)) value |= DATA_REQUEST;
		value ^= SIDE_SELECT; // inverted
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

const byte* VictorFDC::getReadCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FF8 & CacheLine::HIGH)) {
		// FDC at 0x7FF8-0x7FFC
		return nullptr;
	} else if ((0x4000 <= start) && (start < 0x8000)) {
		// ROM at 0x4000-0x7FFF
		return MSXFDC::getReadCacheLine(start);
	} else {
		return unmappedRead;
	}
}

void VictorFDC::writeMem(word address, byte value, EmuTime::param time)
{
	switch (address) {
	case 0x7FF8:
		controller.setCommandReg(value, time);
		break;
	case 0x7FF9:
		controller.setTrackReg(value, time);
		break;
	case 0x7FFA:
		controller.setSectorReg(value, time);
		break;
	case 0x7FFB:
		controller.setDataReg(value, time);
		break;
	case 0x7FFC:
		DriveMultiplexer::DriveNum drive
			= ((value & DRIVE_DISABLE) != 0)
			? DriveMultiplexer::NO_DRIVE
			: (((value & DRIVE_SELECT) != 0) ? DriveMultiplexer::DRIVE_B
			                                 : DriveMultiplexer::DRIVE_A);
		multiplexer.selectDrive(drive, time);
		multiplexer.setSide((value & SIDE_SELECT) != 0);
		multiplexer.setMotor((drive == DriveMultiplexer::DRIVE_A) ? ((value & DRIVE_A_MOTOR) != 0) : ((value & DRIVE_B_MOTOR) != 0), time); // this is not 100% correct: the motors can be controlled independently via bit 0 and 1
		// back up for reading:
		driveControls = value & (DRIVE_A_MOTOR | DRIVE_B_MOTOR | DRIVE_SELECT | SIDE_SELECT | DRIVE_DISABLE);
		break;
	}
}

byte* VictorFDC::getWriteCacheLine(word address) const
{
	if ((address & CacheLine::HIGH) == (0x7FF8 & CacheLine::HIGH)) {
		// FDC at 0x7FF8-0x7FFC
		return nullptr;
	} else {
		return unmappedWrite;
	}
}

bool VictorFDC::allowUnaligned() const
{
	// OK, because this device doesn't call any 'fillDeviceXXXCache()'functions.
	return true;
}


template<typename Archive>
void VictorFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
	ar.serialize("driveControls", driveControls);
}
INSTANTIATE_SERIALIZE_METHODS(VictorFDC);
REGISTER_MSXDEVICE(VictorFDC, "VictorFDC");

} // namespace openmsx
