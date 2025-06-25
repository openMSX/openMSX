#include "YamahaFDC.hh"
#include "CacheLine.hh"
#include "DriveMultiplexer.hh"
#include "MSXException.hh"
#include "Rom.hh"
#include "WD2793.hh"
#include "one_of.hh"
#include "serialize.hh"

// This is derived by disassembly of the Yamaha FD-03 disk rom
//   https://sourceforge.net/p/msxsyssrc/git/ci/master/tree/diskdrvs/fd-03/
//
// FDD interface:
// 7FC0   I/O            FDC STATUS/COMMAND
// 7FC1   I/O            FDC TRACK REGISTER
// 7FC2   I/O            FDC SECTOR REGISTER
// 7FC3   I/O            FDC DATA REGISTER
// 7FE0   O     bit 0    SELECT DRIVE A              "1" ON
//        I     bit 0    READY DRIVE A               "0" READY, "1" NOT READY
//        O     bit 1    SELECT DRIVE B              "1" ON
//        I     bit 1    READY DRIVE B               "0" READY, "1" NOT READY
//        O     bit 2    MOTOR                       "1" ON
//        I     bit 2    DISK CHANGE DRIVE A         "1" CHANGED
//        O     bit 3    UNKNOWN FUNCTION
//        I     bit 3    DISK CHANGE DRIVE B         "1" CHANGED
//        I     bit 6    FDC DATA REQUEST            "1" REQUEST
//        I     bit 7    FDC INTERRUPT REQUEST       "1" REQUEST
//
// 7FF0   O              RESET DISK CHANGE DRIVE A
//        I              RESET DISK CHANGE DRIVE B

namespace openmsx {

static constexpr int DRIVE_A_SELECT = 0x01;
static constexpr int DRIVE_B_SELECT = 0x02;
static constexpr int DRIVE_A_NOT_READY = 0x01;
static constexpr int DRIVE_B_NOT_READY = 0x02;
static constexpr int DISK_A_CHANGED = 0x04;
static constexpr int DISK_B_CHANGED = 0x08;
static constexpr int MOTOR_ON = 0x04;
static constexpr int DATA_REQUEST  = 0x40;
static constexpr int INTR_REQUEST  = 0x80;


YamahaFDC::YamahaFDC(DeviceConfig& config)
	: WD2793BasedFDC(config, "", true, DiskDrive::TrackMode::YAMAHA_FD_03)
{
	if (rom->size() != one_of(0x4000u, 0x8000u)) {
		throw MSXException("YamahaFDC ROM size must be 16kB or 32kB.");
	}
	reset(getCurrentTime());
}

void YamahaFDC::reset(EmuTime time)
{
	WD2793BasedFDC::reset(time);
	writeMem(0x7FE0, 0x00, time);
}

byte YamahaFDC::readMem(uint16_t address, EmuTime time)
{
	switch (address & 0x3FFF) {
	case 0x3FC0:
		return controller.getStatusReg(time);
	case 0x3FC1:
		return controller.getTrackReg(time);
	case 0x3FC2:
		return controller.getSectorReg(time);
	case 0x3FC3:
		return controller.getDataReg(time);
	case 0x3FE0: {
		byte value = 0;
		if (!multiplexer.isDiskInserted(DriveMultiplexer::Drive::A)) value |= DRIVE_A_NOT_READY;
		if (!multiplexer.isDiskInserted(DriveMultiplexer::Drive::B)) value |= DRIVE_B_NOT_READY;
		// note: peekDiskChanged() instead of diskChanged() because we don't want an implicit reset
		if (multiplexer.peekDiskChanged(DriveMultiplexer::Drive::A)) value |= DISK_A_CHANGED;
		if (multiplexer.peekDiskChanged(DriveMultiplexer::Drive::B)) value |= DISK_B_CHANGED;
		if (controller.getIRQ(time))  value |= INTR_REQUEST;
		if (controller.getDTRQ(time)) value |= DATA_REQUEST;
		return value;
	}
	case 0x3FF0:
		multiplexer.diskChanged(DriveMultiplexer::Drive::B); // clear
		[[fallthrough]];
	default:
		return peekMem(address, time);
	}
}

byte YamahaFDC::peekMem(uint16_t address, EmuTime time) const
{
	switch (address & 0x3FFF) {
	case 0x3FC0:
		return controller.peekStatusReg(time);
	case 0x3FC1:
		return controller.peekTrackReg(time);
	case 0x3FC2:
		return controller.peekSectorReg(time);
	case 0x3FC3:
		return controller.peekDataReg(time);
	case 0x3FE0: {
		byte value = 0;
		if (!multiplexer.isDiskInserted(DriveMultiplexer::Drive::A)) value |= DRIVE_A_NOT_READY;
		if (!multiplexer.isDiskInserted(DriveMultiplexer::Drive::B)) value |= DRIVE_B_NOT_READY;
		if (multiplexer.peekDiskChanged(DriveMultiplexer::Drive::A)) value |= DISK_A_CHANGED;
		if (multiplexer.peekDiskChanged(DriveMultiplexer::Drive::B)) value |= DISK_B_CHANGED;
		if (controller.peekIRQ(time))  value |= INTR_REQUEST;
		if (controller.peekDTRQ(time)) value |= DATA_REQUEST;
		return value;
	}
	case 0x3FF0:
		// don't clear on peek
		[[fallthrough]];
	default:
		if (unsigned(address - 0x4000) < rom->size()) {
			return (*rom)[address - 0x4000];
		} else {
			return 255;
		}
	}
}

const byte* YamahaFDC::getReadCacheLine(uint16_t start) const
{
	if ((start & 0x3FFF & CacheLine::HIGH) == (0x3FC0 & CacheLine::HIGH)) {
		// FDC at 0x7FC0-0x7FFF  or  0xBFC0-0xBFFF
		return nullptr;
	} else if (unsigned(start - 0x4000) < rom->size()) {
		return &(*rom)[start - 0x4000];
	} else {
		return unmappedRead.data();
	}
}

void YamahaFDC::writeMem(uint16_t address, byte value, EmuTime time)
{
	switch (address & 0x3fff) {
	case 0x3FC0:
		controller.setCommandReg(value, time);
		break;
	case 0x3FC1:
		controller.setTrackReg(value, time);
		break;
	case 0x3FC2:
		controller.setSectorReg(value, time);
		break;
	case 0x3FC3:
		controller.setDataReg(value, time);
		break;
	case 0x3FE0: {
		auto drive = [&] {
			switch (value & (DRIVE_A_SELECT | DRIVE_B_SELECT)) {
			case DRIVE_A_SELECT:
				return DriveMultiplexer::Drive::A;
			case DRIVE_B_SELECT:
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
		multiplexer.setSide(false);
		multiplexer.setMotor((value & MOTOR_ON) != 0, time);
		break;
	}
	case 0x3FF0:
		multiplexer.diskChanged(DriveMultiplexer::Drive::A);
		break;
	}
}

byte* YamahaFDC::getWriteCacheLine(uint16_t address)
{
	if ((address & 0x3FFF & CacheLine::HIGH) == (0x3FC0 & CacheLine::HIGH)) {
		// FDC at 0x7FC0-0x7FFF  or  0xBFC0-0xBFFF
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void YamahaFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(YamahaFDC);
REGISTER_MSXDEVICE(YamahaFDC, "YamahaFDC");

} // namespace openmsx
