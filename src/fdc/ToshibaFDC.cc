#include "ToshibaFDC.hh"
#include "CacheLine.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "serialize.hh"

namespace openmsx {

ToshibaFDC::ToshibaFDC(const DeviceConfig& config)
	: WD2793BasedFDC(config)
{
}

byte ToshibaFDC::readMem(word address, EmuTime::param time)
{
	byte value;
	switch (address) {
	case 0x7FF0:
		value = controller.getStatusReg(time);
		break;
	case 0x7FF1:
		value = controller.getTrackReg(time);
		break;
	case 0x7FF2:
		value = controller.getSectorReg(time);
		break;
	case 0x7FF3:
		value = controller.getDataReg(time);
		break;
	case 0x7FF7:
		value = 0xFF;
		if (controller.getIRQ(time))  value &= ~0x40;
		if (controller.getDTRQ(time)) value &= ~0x80;
		break;
	default:
		value = ToshibaFDC::peekMem(address, time);
		break;
	}
	return value;
}

byte ToshibaFDC::peekMem(word address, EmuTime::param time) const
{
	byte value;
	switch (address) { // checked on real HW: no mirroring
	case 0x7FF0:
		value = controller.peekStatusReg(time);
		break;
	case 0x7FF1:
		value = controller.peekTrackReg(time);
		break;
	case 0x7FF2:
		value = controller.peekSectorReg(time);
		break;
	case 0x7FF3:
		value = controller.peekDataReg(time);
		break;
	case 0x7FF4:
		// Probably no function. Toshiba disk ROM doesn't read this.
		value = 0xFF;
		break;
	case 0x7FF5:
		value = 0xFE | r7ff5;
		break;
	case 0x7FF6:
		// Disk ROM reads bit 0. TODO what function does it have?
		value = 0xFE | r7ff6;
		break;
	case 0x7FF7:
		value = 0xFF; // unused bits read as 1
		if (controller.peekIRQ(time))  value &= ~0x40;
		if (controller.peekDTRQ(time)) value &= ~0x80;
		break;
	default:
		if (0x4000 <= address && address < 0x8000) {
			// ROM only visible in 0x4000-0x7FFF.
			value = MSXFDC::peekMem(address, time);
		} else {
			value = 0xFF;
		}
		break;
	}
	return value;
}

const byte* ToshibaFDC::getReadCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FF0 & CacheLine::HIGH)) {
		// FDC at 0x7FF0-0x7FF7
		return nullptr;
	} else if (0x4000 <= start && start < 0x8000) {
		// ROM at 0x4000-0x7FFF
		return MSXFDC::getReadCacheLine(start);
	} else {
		return unmappedRead;
	}
}

void ToshibaFDC::writeMem(word address, byte value, EmuTime::param time)
{
	switch (address) {
	case 0x7FF0:
		controller.setCommandReg(value, time);
		break;
	case 0x7FF1:
		controller.setTrackReg(value, time);
		break;
	case 0x7FF2:
		controller.setSectorReg(value, time);
		break;
	case 0x7FF3:
		controller.setDataReg(value, time);
		break;
	case 0x7FF4:
		// Disk ROM only writes the values 0 or 2.
		multiplexer.setMotor((value & 0x02) != 0, time);
		break;
	case 0x7FF5:
		// Disk ROM only writes the values 0 or 1.
		r7ff5 = value & 1;
		multiplexer.selectDrive(r7ff5 ? DriveMultiplexer::DRIVE_B : DriveMultiplexer::DRIVE_A, time);
		break;
	case 0x7FF6:
		// Disk ROM writes the values 0 or 1. TODO what function does it have?
		r7ff6 = value & 1;
		break;
	case 0x7FF7:
		// Probably no function, disk ROM doesn't write to this address.
		break;
	}
}

byte* ToshibaFDC::getWriteCacheLine(word address) const
{
	if ((address & CacheLine::HIGH) == (0x7FF0 & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return unmappedWrite;
	}
}

template<typename Archive>
void ToshibaFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
	ar.serialize("r7ff5", r7ff5);
	ar.serialize("r7ff6", r7ff6);
}
INSTANTIATE_SERIALIZE_METHODS(ToshibaFDC);
REGISTER_MSXDEVICE(ToshibaFDC, "ToshibaFDC");

} // namespace openmsx
