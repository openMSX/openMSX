#include "MicrosolFDC.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "serialize.hh"

namespace openmsx {

MicrosolFDC::MicrosolFDC(const DeviceConfig& config)
	: WD2793BasedFDC(config)
{
}

byte MicrosolFDC::readIO(word port, EmuTime::param time)
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
		value = 0x7F;
		if (controller.getIRQ(time))  value |=  0x80;
		if (controller.getDTRQ(time)) value &= ~0x40;
		break;
	default:
		value = 255;
		break;
	}
	return value;
}

byte MicrosolFDC::peekIO(word port, EmuTime::param time) const
{
	byte value;
	switch (port & 0x07) {
	case 0:
		value = controller.peekStatusReg(time);
		break;
	case 1:
		value = controller.peekTrackReg(time);
		break;
	case 2:
		value = controller.peekSectorReg(time);
		break;
	case 3:
		value = controller.peekDataReg(time);
		break;
	case 4:
		value = 0x7F;
		if (controller.peekIRQ(time))  value |=  0x80;
		if (controller.peekDTRQ(time)) value &= ~0x40;
		break;
	default:
		value = 255;
		break;
	}
	return value;
}

void MicrosolFDC::writeIO(word port, byte value, EmuTime::param time)
{
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
		multiplexer.setSide((value & 0x10) != 0);
		multiplexer.setMotor((value & 0x20) != 0, time);
		break;
	}
}


template<typename Archive>
void MicrosolFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(MicrosolFDC);
REGISTER_MSXDEVICE(MicrosolFDC, "MicrosolFDC");

} // namespace openmsx
