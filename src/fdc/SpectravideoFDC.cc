#include "SpectravideoFDC.hh"
#include "CacheLine.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "MSXException.hh"
#include "serialize.hh"

// Note: although this implementation seems to work, it has not been checked on
// real hardware how the FDC registers are mirrored across the slot, nor how
// the ROM is visible in the slot. Currently FDC registers are implemented to
// be mirrored all over the slot (as it seems that the MSX-DOS that came with
// the SVI-707 needs that), and ROMs are implemented to be visible in page 1.
//
// This implementation is solely based on the MSX SVI-728 Service and Technical
// Manual [Vol.1], page 3-7 (memory mapping of registers) and page 3-1 (ROM).
// Thanks to Leonard Oliveira for interpreting some of the hardware schematics
// in that same manual.
// Thanks to Benoit Delvaux for providing some extra info and software to test
// with.

namespace openmsx {

SpectravideoFDC::SpectravideoFDC(const DeviceConfig& config)
	: WD2793BasedFDC(config, "msxdos")
	, cpmRom(getName() + " CP/M ROM", "rom", config, "cpm")
{
	if (cpmRom.getSize() != 0x1000) {
		throw MSXException("CP/M ROM must be exactly 4kB in size.");
	}
	reset(getCurrentTime());
}

void SpectravideoFDC::reset(EmuTime::param /*time*/)
{
	cpmRomEnabled = true;
}

byte SpectravideoFDC::readMem(word address, EmuTime::param time)
{
	switch (address & 0x3FFF) {
	case 0x3FB8:
		return controller.getStatusReg(time);
	case 0x3FB9:
		return controller.getTrackReg(time);
	case 0x3FBA:
		return controller.getSectorReg(time);
	case 0x3FBB:
		return controller.getDataReg(time);
	case 0x3FBC: {
		byte value = 0;
		if ( controller.getIRQ (time)) value |= 0x80;
		if (!controller.getDTRQ(time)) value |= 0x40;
		return value;
	}
	case 0x3FBE: // Software switch to turn on CP/M,
	             // boot ROM and turn off MSX DOS ROM.
		cpmRomEnabled = true;
		return 0xFF;
	case 0x3FBF: // Software switch to turn off CP/M,
	             // boot ROM and turn on MSX DOS ROM.
		cpmRomEnabled = false;
		return 0xFF;
	default:
		return SpectravideoFDC::peekMem(address, time);
	}
}

byte SpectravideoFDC::peekMem(word address, EmuTime::param time) const
{
	switch (address & 0x3FFF) {
	case 0x3FB8:
		return controller.peekStatusReg(time);
	case 0x3FB9:
		return controller.peekTrackReg(time);
	case 0x3FBA:
		return controller.peekSectorReg(time);
	case 0x3FBB:
		return controller.peekDataReg(time);
	case 0x3FBC: {
		// Drive control IRQ and DRQ lines are not connected to Z80 interrupt request
		// bit 7: interrupt of 1793 (1 for interrupt)
		// bit 6: data request of 1793 (0 for request)
		// TODO: other bits read 0?
		byte value = 0;
		if ( controller.peekIRQ (time)) value |= 0x80;
		if (!controller.peekDTRQ(time)) value |= 0x40;
		return value;
	}
	default:
		if ((0x4000 <= address) && (address < 0x8000) && !cpmRomEnabled) {
			// MSX-DOS ROM only visible in 0x4000-0x7FFF
			return MSXFDC::peekMem(address, time);
		} else if ((0x4000 <= address) && (address < 0x5000) && cpmRomEnabled) {
			// CP/M ROM only visible in 0x4000-0x4FFF
			return cpmRom[address & 0x0FFF];
		} else {
			return 0xFF;
		}
	}
}

const byte* SpectravideoFDC::getReadCacheLine(word start) const
{
	if ((start & 0x3FFF & CacheLine::HIGH) == (0x3FB8 & CacheLine::HIGH)) {
		// FDC at 0x7FB8-0x7FBF, and mirrored in other pages
		return nullptr;
	} else if ((0x4000 <= start) && (start < 0x8000) && !cpmRomEnabled) {
		// MSX-DOS ROM at 0x4000-0x7FFF
		return MSXFDC::getReadCacheLine(start);
	} else if ((0x4000 <= start) && (start < 0x5000) && cpmRomEnabled) {
		// CP/M ROM at 0x4000-0x4FFF
		return &cpmRom[start & 0x0FFF];
	} else {
		return unmappedRead;
	}
}

void SpectravideoFDC::writeMem(word address, byte value, EmuTime::param time)
{
	switch (address & 0x3FFF) {
	case 0x3FB8:
		controller.setCommandReg(value, time);
		break;
	case 0x3FB9:
		controller.setTrackReg(value, time);
		break;
	case 0x3FBA:
		controller.setSectorReg(value, time);
		break;
	case 0x3FBB:
		controller.setDataReg(value, time);
		break;
	case 0x3FBC:
		// bit 0 -> enable drive (1 for ENABLE)
		// bit 2 -> side select (0 for side 0)
		// bit 3 -> motor on (1 for ON)
		multiplexer.selectDrive((value & 0x01) != 0 ? DriveMultiplexer::DRIVE_A : DriveMultiplexer::NO_DRIVE, time);
		multiplexer.setSide    ((value & 0x04) != 0);
		multiplexer.setMotor   ((value & 0x08) != 0, time);
		break;
	case 0x3FBE: // Software switch to turn on CP/M,
	             // boot ROM and turn off MSX DOS ROM.
		cpmRomEnabled = true;
		break;
	case 0x3FBF: // Software switch to turn off CP/M,
	             // boot ROM and turn on MSX DOS ROM.
		cpmRomEnabled = false;
		break;
	}
}

byte* SpectravideoFDC::getWriteCacheLine(word address) const
{
	if ((address & 0x3FFF & CacheLine::HIGH) == (0x3FB8 & CacheLine::HIGH)) {
		// FDC at 0x7FB8-0x7FBF - mirrored
		return nullptr;
	} else {
		return unmappedWrite;
	}
}

template<typename Archive>
void SpectravideoFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
	ar.serialize("cpmRomEnabled", cpmRomEnabled);
}
INSTANTIATE_SERIALIZE_METHODS(SpectravideoFDC);
REGISTER_MSXDEVICE(SpectravideoFDC, "SpectravideoFDC");

} // namespace openmsx
