// $Id$

#include "MicrosolFDC.hh"
#include "WD2793.hh"
#include "DriveMultiplexer.hh"

namespace openmsx {

MicrosolFDC::MicrosolFDC(const XMLElement& config, const EmuTime& time)
	: WD2793BasedFDC(config, time)
{
}

MicrosolFDC::~MicrosolFDC()
{
}

byte MicrosolFDC::readIO(byte port, const EmuTime& time)
{
	byte value;
	switch (port & 0x07) {
	case 0:
		value = controller.getStatusReg(time);
		break;
	case 1:
		value = controller.getTrackReg(time);
		break;
	case 2:
		value = controller.getSectorReg(time);
		break;
	case 3:
		value = controller.getDataReg(time);
		break;
	case 4:
		value = driveD4;
		break;
	default:
		// This port should not have been registered.
		assert(false);
		value = 255;
		break;
	}
	PRT_DEBUG("MicrosolFDC: read 0x" << hex << (int)port << " 0x" << (int)value << dec);
	return value;
}

byte MicrosolFDC::peekIO(byte port, const EmuTime& time) const
{
	// TODO peekIO not implemented
	return 0xFF;
}

void MicrosolFDC::writeIO(byte port, byte value, const EmuTime& time)
{
	PRT_DEBUG("MicrosolFDC: write 0x" << hex << (int)port << " 0x" << (int)value << dec);
	switch (port & 0x07) {
	case 0:
		controller.setCommandReg(value, time);
		break;
	case 1:
		controller.setTrackReg(value, time);
		break;
	case 2:
		controller.setSectorReg(value, time);
		break;
	case 3:
		controller.setDataReg(value, time);
		break;
	case 4:
		// From Ricardo Bittencourt
		// bit 0:  drive select A
		// bit 1:  drive select B
		// bit 2:  drive select C
		// bit 3:  drive select D
		// bit 4:  side select
		// bit 5:  turn on motor
		// bit 6:  enable waitstates
		// bit 7:  density: 0=single 1=double
		//
		// When you enable a drive select bit, the led on the
		// disk-drive turns on. Since this was used as user feedback,
		// in things such as "replace disk 1 when the led turns off"
		// we need to connect this to the OSD later on.

		driveD4 = value;
		// Set correct drive
		DriveMultiplexer::DriveNum drive;
		switch (value & 0x0F) {
		case 1:
			drive = DriveMultiplexer::DRIVE_A;
			break;
		case 2:
			drive = DriveMultiplexer::DRIVE_B;
			break;
		case 4:
			drive = DriveMultiplexer::DRIVE_C;
			break;
		case 8:
			drive = DriveMultiplexer::DRIVE_D;
			break;
		default:
			// No drive selected or two drives at same time
			// The motor is enabled for all drives at the same time, so
			// in a real machine you must take care to do not select more
			// than one drive at the same time (you could get data
			// collision).
			drive = DriveMultiplexer::NO_DRIVE;
		}
		multiplexer.selectDrive(drive, time);
		multiplexer.setSide(value & 0x10);
		multiplexer.setMotor(value & 0x20, time);
		break;
	}
}

} // namespace openmsx
