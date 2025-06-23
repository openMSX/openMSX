#include "AVTFDC.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "serialize.hh"

namespace openmsx {

AVTFDC::AVTFDC(DeviceConfig& config)
	: WD2793BasedFDC(config)
{
}

byte AVTFDC::readIO(uint16_t port, EmuTime::param time)
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

byte AVTFDC::peekIO(uint16_t port, EmuTime::param time) const
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

void AVTFDC::writeIO(uint16_t port, byte value, EmuTime::param time)
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
	case 4: /* nothing only read... */
		break;
	case 5:
		// From mohai
		// bit 0:  drive select A (and motor on, as this is a WD1770,
		// we use this as workaround)
		// bit 1:  drive select B (and motor on, as this is a WD1770,
		// we use this as workaround)
		// bit 2:  side select
		// bit 3:  density: 1=single 0=double (not supported by openMSX)
		//
		// Set correct drive
		auto drive = [&] {
			switch (value & 0x03) {
			case 1:
				return DriveMultiplexer::Drive::A;
			case 2:
				return DriveMultiplexer::Drive::B;
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
		multiplexer.setSide((value & 0x04) != 0);
		multiplexer.setMotor(drive != DriveMultiplexer::Drive::NONE, time);
		break;
	}
}


template<typename Archive>
void AVTFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(AVTFDC);
REGISTER_MSXDEVICE(AVTFDC, "AVTFDC");

} // namespace openmsx
