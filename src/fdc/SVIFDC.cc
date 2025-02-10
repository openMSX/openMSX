#include "SVIFDC.hh"
#include "serialize.hh"

// Floppy disk controller FD1793
//
// 30H         Command to FDC
// 30H         Status from FDC
// 31H         Track register
// 32H         Sector register
// 33H         Data register
//
// 34H         Address of INTRQ and DRQ
// 10000000B   Interrupt request bit
// 01000000B   Data request bit
//
// 34H         Address of disk select and motor control
// 00000001B   Select disk 0 bit
// 00000010B   Select disk 1 bit
// 00000100B   Disk 0 motor on bit
// 00001000B   Disk 1 motor on bit
//
// 38H         Address of density select flag
// 00000000B   Density MFM bit
// 00000001B   Density FM bit
// 00000010B   Select second side

namespace openmsx {

SVIFDC::SVIFDC(DeviceConfig& config)
	: WD2793BasedFDC(config, "", false) // doesn't require a <rom>
{
}

byte SVIFDC::readIO(word port, EmuTime::param time)
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
	case 4:
		return (controller.getIRQ (time) ? 0x80 : 0x00) |
		       (controller.getDTRQ(time) ? 0x40 : 0x00);
	}
	return 255;
}

byte SVIFDC::peekIO(word port, EmuTime::param time) const
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
	case 4:
		return (controller.peekIRQ (time) ? 0x80 : 0x00) |
		       (controller.peekDTRQ(time) ? 0x40 : 0x00);
	}
	return 255;
}

void SVIFDC::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0x0F) {
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
	case 4: {
		// bit 0:  drive select A
		// bit 1:  drive select B
		// bit 2:  motor on drive A
		// bit 3:  motor on drive B
		auto [drive, motor] = [&]() -> std::pair<DriveMultiplexer::Drive, bool> {
			switch (value & 0x03) {
			case 1:
				return {DriveMultiplexer::Drive::A, value & 0x04};
			case 2:
				return {DriveMultiplexer::Drive::B, value & 0x08};
			default:
				// No drive selected or two drives at same time
				// In a real machine you must take care to do not select more
				// than one drive at the same time (you could get data
				// collision).
				return {DriveMultiplexer::Drive::NONE, false};
			}
		}();
		multiplexer.selectDrive(drive, time);
		multiplexer.setMotor(motor, time);
		break;
	}
	case 8:
		// bit 0:  density flag (1=single, 0=double)
		// bit 1:  side select
		// TODO density flag not yet supported
		multiplexer.setSide((value & 0x02) != 0);
		break;
	}
}

template<typename Archive>
void SVIFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(SVIFDC);
REGISTER_MSXDEVICE(SVIFDC, "SVI-328 FDC");

} // namespace openmsx
