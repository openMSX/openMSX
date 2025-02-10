#include "MicrosolFDC.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "serialize.hh"

namespace openmsx {

MicrosolFDC::MicrosolFDC(DeviceConfig& config)
	: WD2793BasedFDC(config)
{
}

byte MicrosolFDC::readIO(word port, EmuTime::param time)
{
	switch (port & 0x07) {
	case 0:
		return controller.getStatusReg(time);
	case 1:
		return controller.getTrackReg(time);
	case 2:
		return controller.getSectorReg(time);
	case 3:
		return controller.getDataReg(time);
	case 4: {
		byte value = 0x7F;
		if (controller.getIRQ(time))  value |=  0x80;
		if (controller.getDTRQ(time)) value &= ~0x40;
		return value;
	}
	default:
		return 255;
	}
}

byte MicrosolFDC::peekIO(word port, EmuTime::param time) const
{
	switch (port & 0x07) {
	case 0:
		return controller.peekStatusReg(time);
	case 1:
		return controller.peekTrackReg(time);
	case 2:
		return controller.peekSectorReg(time);
	case 3:
		return controller.peekDataReg(time);
	case 4: {
		byte value = 0x7F;
		if (controller.peekIRQ(time))  value |=  0x80;
		if (controller.peekDTRQ(time)) value &= ~0x40;
		return value;
	}
	default:
		return 255;
	}
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
		// bit 6:  enable wait states
		// bit 7:  density: 0=single 1=double
		//
		// When you enable a drive select bit, the led on the
		// disk-drive turns on. Since this was used as user feedback,
		// in things such as "replace disk 1 when the led turns off"
		// we need to connect this to the OSD later on.

		// Set correct drive
		auto drive = [&] {
			switch (value & 0x0F) {
			case 1:
				return DriveMultiplexer::Drive::A;
			case 2:
				return DriveMultiplexer::Drive::B;
			case 4:
				return DriveMultiplexer::Drive::C;
			case 8:
				return DriveMultiplexer::Drive::D;
			default:
				// No drive selected or two drives at same time
				// The motor is enabled for all drives at the same time, so
				// in a real machine you must take care to do not select more
				// than one drive at the same time (you could get data
				// collision).
				return DriveMultiplexer::Drive::NONE;
			}
		}();
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
