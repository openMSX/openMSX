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

SanyoFDC::SanyoFDC(DeviceConfig& config)
	: WD2793BasedFDC(config)
{
	// ROM only visible in 0x0000-0x7FFF by default (not verified!)
	parseRomVisibility(config, 0x0000, 0x8000);
}

byte SanyoFDC::readMem(word address, EmuTime::param time)
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
	case 0x7FFC:
	case 0x7FFD:
	case 0x7FFE:
	case 0x7FFF: {
		byte value = 0x3F;
		if (controller.getIRQ(time))  value |= 0x80;
		if (controller.getDTRQ(time)) value |= 0x40;
		return value;
	}
	default:
		return SanyoFDC::peekMem(address, time);
	}
}

byte SanyoFDC::peekMem(word address, EmuTime::param time) const
{
	switch (address) {
	case 0x7FF8:
		return controller.peekStatusReg(time);
		break;
	case 0x7FF9:
		return controller.peekTrackReg(time);
		break;
	case 0x7FFA:
		return controller.peekSectorReg(time);
		break;
	case 0x7FFB:
		return controller.peekDataReg(time);
		break;
	case 0x7FFC:
	case 0x7FFD:
	case 0x7FFE:
	case 0x7FFF: {
		// Drive control IRQ and DRQ lines are not connected to Z80 interrupt request
		// bit 7: intrq
		// bit 6: dtrq
		// other bits read 1
		byte value = 0x3F;
		if (controller.peekIRQ(time))  value |= 0x80;
		if (controller.peekDTRQ(time)) value |= 0x40;
		return value;
	}
	default:
		return MSXFDC::peekMem(address, time);
	}
}

const byte* SanyoFDC::getReadCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FF8 & CacheLine::HIGH)) {
		// FDC at 0x7FF8-0x7FFC - mirroring behaviour unknown
		return nullptr;
	} else {
		return MSXFDC::getReadCacheLine(start);
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
		auto drive = [&] {
			switch (value & 3) {
				case 1:  return DriveMultiplexer::Drive::A;
				case 2:  return DriveMultiplexer::Drive::B;
				default: return DriveMultiplexer::Drive::NONE;
			}
		}();
		multiplexer.selectDrive(drive, time);
		multiplexer.setSide((value & 0x04) != 0);
		multiplexer.setMotor((value & 0x08) != 0, time);
		break;
	}
}

byte* SanyoFDC::getWriteCacheLine(word address)
{
	if ((address & CacheLine::HIGH) == (0x7FF8 & CacheLine::HIGH)) {
		// FDC at 0x7FF8-0x7FFC - mirroring behaviour unknown
		return nullptr;
	} else {
		return unmappedWrite.data();
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
