#include "ToshibaFDC.hh"
#include "CacheLine.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "serialize.hh"

// Based on studying the code in the Toshiba disk ROM.
// Thanks to Arjen Zeilemaker for an annotated disassembly:
//   https://sourceforge.net/p/msxsyssrc/git/ci/master/tree/diskdrvs/hx-f101/driver.mac
//   https://sourceforge.net/p/msxsyssrc/git/ci/master/tree/diskdrvs/hx-34/driver.mac

namespace openmsx {

ToshibaFDC::ToshibaFDC(DeviceConfig& config)
	: WD2793BasedFDC(config)
{
	// ROM only visible in 0x4000-0x7FFF by default
	parseRomVisibility(config, 0x4000, 0x4000);

}

byte ToshibaFDC::readMem(word address, EmuTime::param time)
{
	switch (address) {
	case 0x7FF0:
		return controller.getStatusReg(time);
	case 0x7FF1:
		return controller.getTrackReg(time);
	case 0x7FF2:
		return controller.getSectorReg(time);
	case 0x7FF3:
		return controller.getDataReg(time);
	case 0x7FF6:
		return 0xFE | (multiplexer.diskChanged() ? 0 : 1);
	case 0x7FF7: {
		byte value = 0xFF;
		if (controller.getIRQ(time))  value &= ~0x40;
		if (controller.getDTRQ(time)) value &= ~0x80;
		return value;
	}
	default:
		return ToshibaFDC::peekMem(address, time);
	}
}

byte ToshibaFDC::peekMem(word address, EmuTime::param time) const
{
	switch (address) { // checked on real HW: no mirroring
	case 0x7FF0:
		return controller.peekStatusReg(time);
	case 0x7FF1:
		return controller.peekTrackReg(time);
	case 0x7FF2:
		return controller.peekSectorReg(time);
	case 0x7FF3:
		return controller.peekDataReg(time);
	case 0x7FF4:
		// ToshibaFDC HX-F101 disk ROM doesn't read this, but HX-34
		// does. No idea if this location is actually readable on
		// HX-F101, but currently the emulation is the same for both.
		return 0xFC
		      | (multiplexer.getSide() ? 1 : 0)
		      | (multiplexer.getMotor() ? 2 : 0);
	case 0x7FF5:
		return 0xFE | ((multiplexer.getSelectedDrive() == DriveMultiplexer::Drive::B) ? 1 : 0);
	case 0x7FF6:
		return 0xFE | (multiplexer.peekDiskChanged() ? 0 : 1);
	case 0x7FF7: {
		byte value = 0xFF; // unused bits read as 1
		if (controller.peekIRQ(time))  value &= ~0x40;
		if (controller.peekDTRQ(time)) value &= ~0x80;
		return value;
	}
	default:
		return MSXFDC::peekMem(address, time);
	}
}

const byte* ToshibaFDC::getReadCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FF0 & CacheLine::HIGH)) {
		// FDC at 0x7FF0-0x7FF7
		return nullptr;
	} else {
		return MSXFDC::getReadCacheLine(start);
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
		multiplexer.setSide((value & 0x01) != 0); // no effect on HX-F101 because it has single sided drive
		multiplexer.setMotor((value & 0x02) != 0, time);
		break;
	case 0x7FF5:
		// Disk ROM only writes the values 0 or 1.
		multiplexer.selectDrive((value & 1) ? DriveMultiplexer::Drive::B
		                                    : DriveMultiplexer::Drive::A,
		                        time);
		break;
	case 0x7FF6:
		// Disk ROM writes '1' (to drive A) and shortly after '0' (to drive B).
		// TODO What does this do? Activate the 'disk is changed' state?
		//      And if so, does the written value matter?
		break;
	case 0x7FF7:
		// Probably no function, disk ROM doesn't write to this address.
		break;
	}
}

byte* ToshibaFDC::getWriteCacheLine(word address)
{
	if ((address & CacheLine::HIGH) == (0x7FF0 & CacheLine::HIGH)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void ToshibaFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(ToshibaFDC);
REGISTER_MSXDEVICE(ToshibaFDC, "ToshibaFDC");

} // namespace openmsx
