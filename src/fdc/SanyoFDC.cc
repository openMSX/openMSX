#include "SanyoFDC.hh"
#include "CacheLine.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "serialize.hh"

// Note: although this implementation seems to work (e.g. for the Sanyo
// MFD-001), it has not been checked on real hardware how the FDC registers are
// mirrored across the slot, nor how the ROM is visible in the slot. Currently
// FDC registers are implemented to be not mirrored, and ROM is implemented to
// be visible in page 0 and 1.

namespace openmsx {

SanyoFDC::SanyoFDC(const DeviceConfig& config)
	: WD2793BasedFDC(config)
{
}

byte SanyoFDC::readMem(word address, EmuTime::param time)
{
	byte value;
	switch (address) {
	case 0x7FF8:
		value = controller.getStatusReg(time);
		break;
	case 0x7FF9:
		value = controller.getTrackReg(time);
		break;
	case 0x7FFA:
		value = controller.getSectorReg(time);
		break;
	case 0x7FFB:
		value = controller.getDataReg(time);
		break;
	case 0x7FFC:
	case 0x7FFD:
	case 0x7FFE:
	case 0x7FFF:
		value = 0x3F;
		if (controller.getIRQ(time))  value |= 0x80;
		if (controller.getDTRQ(time)) value |= 0x40;
		break;
	default:
		value = SanyoFDC::peekMem(address, time);
		break;
	}
	return value;
}

byte SanyoFDC::peekMem(word address, EmuTime::param time) const
{
	byte value;
	switch (address) {
	case 0x7FF8:
		value = controller.peekStatusReg(time);
		break;
	case 0x7FF9:
		value = controller.peekTrackReg(time);
		break;
	case 0x7FFA:
		value = controller.peekSectorReg(time);
		break;
	case 0x7FFB:
		value = controller.peekDataReg(time);
		break;
	case 0x7FFC:
	case 0x7FFD:
	case 0x7FFE:
	case 0x7FFF:
		// Drive control IRQ and DRQ lines are not connected to Z80 interrupt request
		// bit 7: intrq
		// bit 6: dtrq
		// other bits read 1
		value = 0x3F;
		if (controller.peekIRQ(time))  value |= 0x80;
		if (controller.peekDTRQ(time)) value |= 0x40;
		break;
	default:
		if (address < 0x8000) {
			// ROM only visible in 0x0000-0x7FFF (not verified!)
			value = MSXFDC::peekMem(address, time);
		} else {
			value = 255;
		}
		break;
	}
	return value;
}

const byte* SanyoFDC::getReadCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FF8 & CacheLine::HIGH)) {
		// FDC at 0x7FF8-0x7FFC - mirroring behaviour unknown
		return nullptr;
	} else if (start < 0x8000) {
		// ROM at 0x0000-0x7FFF (this is a guess, not checked!)
		return MSXFDC::getReadCacheLine(start);
	} else {
		return unmappedRead;
	}
}

void SanyoFDC::writeMem(word address, byte value, EmuTime::param time)
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
	case 0x7FFD:
	case 0x7FFE:
	case 0x7FFF:
		// bit 0 -> select drive 0
		// bit 1 -> select drive 1
		// bit 2 -> side select
		// bit 3 -> motor on
		DriveMultiplexer::DriveNum drive;
		switch (value & 3) {
			case 1:
				drive = DriveMultiplexer::DRIVE_A;
				break;
			case 2:
				drive = DriveMultiplexer::DRIVE_B;
				break;
			default:
				drive = DriveMultiplexer::NO_DRIVE;
		}
		multiplexer.selectDrive(drive, time);
		multiplexer.setSide((value & 0x04) != 0);
		multiplexer.setMotor((value & 0x08) != 0, time);
		break;
	}
}

byte* SanyoFDC::getWriteCacheLine(word address) const
{
	if ((address & CacheLine::HIGH) == (0x7FF8 & CacheLine::HIGH)) {
		// FDC at 0x7FF8-0x7FFC - mirroring behaviour unknown
		return nullptr;
	} else {
		return unmappedWrite;
	}
}


template<typename Archive>
void SanyoFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(SanyoFDC);
REGISTER_MSXDEVICE(SanyoFDC, "SanyoFDC");

} // namespace openmsx
