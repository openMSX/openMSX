// $Id$

#include "NationalFDC.hh"
#include "WD2793.hh"
#include "CPU.hh"


NationalFDC::NationalFDC(MSXConfig::Device *config, const EmuTime &time)
	: MSXFDC(config, time), MSXDevice(config, time)
{
	emptyRom = new byte[CPU::CACHE_LINE_SIZE];
	memset(emptyRom, 255, CPU::CACHE_LINE_SIZE);
	
	// real thing uses MB8877A, but it is compatible with WD2793
	//  TODO check completely compatible
	controller = new WD2793(config);
}

NationalFDC::~NationalFDC()
{
	delete controller;
	delete[] emptyRom;
}

void NationalFDC::reset(const EmuTime &time)
{
	controller->reset();
}

byte NationalFDC::readMem(word address, const EmuTime &time)
{
	byte value = 255;
	switch (address & 0x3FFF) {
	case 0x3FB8:
		value = controller->getStatusReg(time);
		break;
	case 0x3FB9:
		value = controller->getTrackReg(time);
		break;
	case 0x3FBA:
		value = controller->getSectorReg(time);
		break;
	case 0x3FBB:
		value = controller->getDataReg(time);
		break;
	case 0x3FBC:
		// Drive control IRQ and DRQ lines are not connected to Z80 interrupt request
		// bit 7: !intrq
		// bit 6: !dtrq
		value = 0xC0;
		if (controller->getIRQ(time))  value &= ~0x80;
		if (controller->getDTRQ(time)) value &= ~0x40;
		break;
	default:
		if (address < 0x8000) {
			// ROM only visible in 0x0000-0x7FFF
			value = NationalFDC::readMem(address, time);
		}
		break;
	}
	//PRT_DEBUG("NationalFDC::readMem(0x" << std::hex << (int)address << std::dec << ").");
	return value;
}


void NationalFDC::writeMem(word address, byte value, const EmuTime &time)
{
	//PRT_DEBUG("NationalFDC::writeMem(0x" << std::hex << (int)address << std::dec << ", value "<<(int)value<<").");
	switch (address & 0x3FFF) {
	case 0x3FB8:
		controller->setCommandReg(value, time);
		break;
	case 0x3FB9:
		controller->setTrackReg(value, time);
		break;
	case 0x3FBA:
		controller->setSectorReg(value, time);
		break;
	case 0x3FBB:
		controller->setDataReg(value, time);
		break;
	case 0x3FBC:
		//bit 1,0 -> drive number  (00 or 10: drive A, 01: drive B, 11: nothing)
		//bit 2   -> side select
		//bit 3   -> motor on
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
		controller->setSideSelect((value & 0x04) ? 1 : 0, time);
		controller->setMotor((value & 0x08) ? 1 : 0, time);
		break;
	}
}

byte* NationalFDC::getReadCacheLine(word start)
{
	if ((start & 0x3FB8 & CPU::CACHE_LINE_HIGH) == (0x3FB8 & CPU::CACHE_LINE_HIGH))
		// FDC at 0x7FB8-0x7FBC (also mirrored)
		return NULL;
	if (start < 0x8000) {
		// ROM at 0x0000-0x7FFF
		return MSXFDC::getReadCacheLine(start);
	} else {
		return emptyRom;
	}
}
