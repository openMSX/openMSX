// $Id$

#include "PhilipsFDC.hh"
#include "WD2793.hh"
#include "CPU.hh"


PhilipsFDC::PhilipsFDC(MSXConfig::Device *config, const EmuTime &time)
	: MSXFDC(config, time), MSXDevice(config, time)
{
	emptyRom = new byte[CPU::CACHE_LINE_SIZE];
	memset(emptyRom, 255, CPU::CACHE_LINE_SIZE);
	
	controller = new WD2793(config);
	
	if (deviceConfig->hasParameter("brokenFDCread")) {
		brokenFDCread = deviceConfig->getParameterAsBool("brokenFDCread");
	} else {
		brokenFDCread = false;
	}
}

PhilipsFDC::~PhilipsFDC()
{
	delete controller;
	delete[] emptyRom;
}

void PhilipsFDC::reset(const EmuTime &time)
{
	controller->reset();
}

byte PhilipsFDC::readMem(word address, const EmuTime &time)
{
	byte value;
	switch (address & 0x3FFF) {
	case 0x3FF8:
		value = controller->getStatusReg(time);
		break;
	case 0x3FF9:
		value = brokenFDCread ? 255 : controller->getTrackReg(time);
		//TODO: check if such broken interfaces indeed return 255 or something else
		// example of such machines : Sony 700 series
		break;
	case 0x3FFA:
		value = brokenFDCread ? 255 : controller->getSectorReg(time);
		//TODO: check if such broken interfaces indeed return 255 or something else
		break;
	case 0x3FFB:
		value = controller->getDataReg(time);
		break;
	case 0x3FFC:
		//bit 0 = side select
		//TODO check other bits !!
		value = controller->getSideSelect(time);
		break;
	case 0x3FFD:
		//bit 1,0 -> drive number  (00 or 10: drive A, 01: drive B, 11: nothing)
		//bit 7 -> motor on
		//TODO check other bits !!
		value = driveReg; //controller->getDriveSelect(time);
		break;
	case 0x3FFE:
		//not used
		value = 255;
		break;
	case 0x3FFF:
		// Drive control IRQ and DRQ lines are not connected to Z80 interrupt request
		// bit 6: !intrq
		// bit 7: !dtrq
		//TODO check other bits !!
		value = 0xC0;
		if (controller->getIRQ(time)) value &= ~0x40;
		if (controller->getDTRQ(time)) value &= ~0x80;
		break;

	default:
		if (address < 0x8000) {
			// ROM only visible in 0x0000-0x7FFF
			value = MSXFDC::readMem(address, time);
		} else {
			value = 255;
		}
		break;
	}
	//PRT_DEBUG("PhilipsFDC::readMem(0x" << std::hex << (int)address << std::dec << ").");
	return value;
}


void PhilipsFDC::writeMem(word address, byte value, const EmuTime &time)
{
	//PRT_DEBUG("PhilipsFDC::writeMem(0x" << std::hex << (int)address << std::dec << ", value "<<(int)value<<").");
	switch (address & 0x3FFF) {
	case 0x3FF8:
		controller->setCommandReg(value, time);
		break;
	case 0x3FF9:
		controller->setTrackReg(value, time);
		break;
	case 0x3FFA:
		controller->setSectorReg(value, time);
		break;
	case 0x3FFB:
		controller->setDataReg(value, time);
		break;
	case 0x3FFC:
		//bit 0 = side select
		//TODO check other bits !!
		controller->setSideSelect(value&1, time);
		break;
	case 0x3FFD:
		//bit 1,0 -> drive number  (00 or 10: drive A, 01: drive B, 11: nothing)
		//bit 7 -> motor on
		//TODO check other bits !!
		driveReg = value;
		byte drivenr;
		switch (value & 3) {
			case 0:
			case 2:
				drivenr = 0;
				break;
			case 1:
				drivenr = 1;
				break;
			case 3:
			default:
				drivenr = 255; //no drive selected
		}
		controller->setDriveSelect(drivenr, time);
		controller->setMotor((value & 128) ? 1 : 0, time); // set motor for current drive
		break;
	}
}

byte* PhilipsFDC::getReadCacheLine(word start)
{
	//if address overlap 0x7ff8-0x7ffb then return NULL, else normal ROM behaviour
	if ((start & 0x3FF8 & CPU::CACHE_LINE_HIGH) == (0x3FF8 & CPU::CACHE_LINE_HIGH))
		return NULL;
	if (start < 0x8000) {
		// ROM visible in 0x0000-0x7FFF
		return MSXFDC::getReadCacheLine(start);
	} else {
		return emptyRom;
	}
}
