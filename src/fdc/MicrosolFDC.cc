// $Id$

#include "MicrosolFDC.hh"
#include "WD2793.hh"
#include "MSXCPUInterface.hh"
#include "DriveMultiplexer.hh"


MicrosolFDC::MicrosolFDC(MSXConfig::Device *config, const EmuTime &time)
	: WD2793BasedFDC(config, time), MSXDevice(config, time),
	  MSXIODevice(config, time)
{
	MSXCPUInterface::instance()->register_IO_In (0xD0,this);
	MSXCPUInterface::instance()->register_IO_In (0xD1,this);
	MSXCPUInterface::instance()->register_IO_In (0xD2,this);
	MSXCPUInterface::instance()->register_IO_In (0xD3,this);
	MSXCPUInterface::instance()->register_IO_In (0xD4,this);
	MSXCPUInterface::instance()->register_IO_Out(0xD0,this);
	MSXCPUInterface::instance()->register_IO_Out(0xD1,this);
	MSXCPUInterface::instance()->register_IO_Out(0xD2,this);
	MSXCPUInterface::instance()->register_IO_Out(0xD3,this);
	MSXCPUInterface::instance()->register_IO_Out(0xD4,this);
}

MicrosolFDC::~MicrosolFDC()
{
}

byte MicrosolFDC::readIO(byte port, const EmuTime &time)
{
	byte value;
	switch (port) {
	case 0xD0:
		value = controller->getStatusReg(time);
		break;
	case 0xD1:
		value = controller->getTrackReg(time);
		break;
	case 0xD2:
		value = controller->getSectorReg(time);
		break;
	case 0xD3:
		value = controller->getDataReg(time);
		break;
	case 0xD4:
		value = driveD4;
		break;
	default:
		assert(false);
		value = 255;
		break;
	}
	PRT_DEBUG("MicrosolFDC: read 0x" << std::hex << (int)port << " 0x" << (int)value << std::dec);
	return value;
}

void MicrosolFDC::writeIO(byte port, byte value, const EmuTime &time)
{
	PRT_DEBUG("MicrosolFDC: write 0x" << std::hex << (int)port << " 0x" << (int)value << std::dec);
	switch (port) {
	case 0xD0:
		controller->setCommandReg(value, time);
		break;
	case 0xD1:
		controller->setTrackReg(value, time);
		break;
	case 0xD2:
		controller->setSectorReg(value, time);
		break;
	case 0xD3:
		controller->setDataReg(value, time);
		break;
	case 0xD4:
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

		driveD4 = value;
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
		multiplexer->selectDrive(drive);
		multiplexer->setSide(value & 0x10);
		multiplexer->setMotor((value & 0x20), time);
		break;
	}
}
