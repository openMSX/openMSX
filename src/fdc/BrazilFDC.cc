// $Id$

#include "BrazilFDC.hh"
#include "WD2793.hh"
#include "MSXCPUInterface.hh"


BrazilFDC::BrazilFDC(MSXConfig::Device *config, const EmuTime &time)
	: MSXFDC(config, time), MSXDevice(config, time), MSXIODevice(config, time)
{
	controller = new WD2793(config);
	
	if (deviceConfig->hasParameter("brokenFDCread")) {
		brokenFDCread = deviceConfig->getParameterAsBool("brokenFDCread");
	} else {
		brokenFDCread = false;
	}
	
	MSXCPUInterface::instance()->register_IO_In (0xD0,this);
	MSXCPUInterface::instance()->register_IO_In (0xD1,this);
	MSXCPUInterface::instance()->register_IO_In (0xD2,this);
	MSXCPUInterface::instance()->register_IO_In (0xD3,this);
	MSXCPUInterface::instance()->register_IO_In (0xD4,this);
}

BrazilFDC::~BrazilFDC()
{
	delete controller;
}

void BrazilFDC::reset(const EmuTime &time)
{
	controller->reset();
}

byte BrazilFDC::readIO(byte port, const EmuTime &time)
{
	byte value = 255;
	switch (port) {
	case 0xD0:
		value = controller->getStatusReg(time);
		break;
	case 0xD1:
		value = brokenFDCread ? 255 : controller->getTrackReg(time);
		break;
	case 0xD2:
		value = brokenFDCread ? 255 : controller->getSectorReg(time);
		break;
	case 0xD3:
		value = controller->getDataReg(time);
		break;
	case 0xD4:
		value = driveD4;
		break;
	}
	return value;
}

void BrazilFDC::writeIO(byte port, byte value, const EmuTime &time)
{
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
		byte drivenr;
		switch (value & 15) {
			case 1:
				drivenr = 0;
				break;
			case 2:
				drivenr = 1;
				break;
			case 4:
				drivenr = 2;
				break;
			case 8:
				drivenr = 3;
				break;
			default:
				// No drive selected or two drives at same time
				// The motor is enabled for all drives at the same time, so
				// in a real machine you must take care to do not select more
				// than one drive at the same time (you could get data
				// collision).
				drivenr = 255; //no drive selected
		}
		controller->setDriveSelect(drivenr, time);
		controller->setSideSelect((value & 16) ? 1 : 0, time);
		controller->setMotor((value & 32) ? 1 : 0, time);
		break;
	}
}
