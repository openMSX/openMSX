// $Id$

#include "MSXFDC.hh"
#include "FDC2793.hh"
#include "CPU.hh"
#include "MSXCPUInterface.hh"


MSXFDC::MSXFDC(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXRom(config, time), MSXIODevice(config, time)
{
	// The loading of the diskrom and the mapping in the slot layout
	// has been done by the MSXRom

	emptyRom = new byte[CPU::CACHE_LINE_SIZE];
	memset(emptyRom, 255, CPU::CACHE_LINE_SIZE);

	std::string fdcChip = deviceConfig->getParameter("chip");
	PRT_DEBUG("FDC of chiptype " << fdcChip);
	if (fdcChip == "2793")
		controller = new FDC2793(config);
	else
		PRT_ERROR("Unknown FDC chiptype");
	
	try {
		brokenFDCread = deviceConfig->getParameterAsBool("brokenFDCread");
		PRT_DEBUG("brokenFDCread " << brokenFDCread);
	} catch(MSXConfig::Exception& e) {
		brokenFDCread = false;
	}
	
	try {
		std::string interfaceType= deviceConfig->getParameter("interface");
		PRT_DEBUG("interfaceType " << interfaceType);
		if      (interfaceType == "memory")
			interface = 0;
		else if (interfaceType == "port")
			interface = 1;
		else if (interfaceType == "hybrid")
			interface = 2;
		else
			assert(false);
	} catch(MSXConfig::Exception& e) {
		interface = 2;
	}

	if (interface) {
		//register extra IO ports for brazilian based interfaces
		MSXCPUInterface::instance()->register_IO_In (0xD0,this);
		MSXCPUInterface::instance()->register_IO_In (0xD1,this);
		MSXCPUInterface::instance()->register_IO_In (0xD2,this);
		MSXCPUInterface::instance()->register_IO_In (0xD3,this);
	}
}

MSXFDC::~MSXFDC()
{
	PRT_DEBUG("Destructing an MSXFDC object");
	delete controller;
	delete[] emptyRom;
}

void MSXFDC::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
	controller->reset();
}

byte MSXFDC::readIO(byte port, const EmuTime &time)
{
	byte value = 255;
	switch (port){
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
	};
	return value;
}

byte MSXFDC::readMem(word address, const EmuTime &time)
{
	byte value = 255;
	//PRT_DEBUG("MSXFDC::readMem(0x" << std::hex << (int)address << std::dec << ").");
	if (interface != 1) {
		// if hybrid or memory based
		//  if address overlap 0x7ff8-0x7ffb then return FDC, else normal ROM behaviour
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
			//PRT_DEBUG("Got byte from disk : "<<(int)value);
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
			if (controller->getIRQ(time)) value &= 0xBF ;
			if (controller->getDTRQ(time)) value &= 0x7F ;
			break;

		default:
			if (address < 0x8000)
				value = romBank[address & 0x3FFF];
			// quick hack to have FDC register in the correct ranges but not the rom
			// (other wise calculus says to litle TPA memory :-)
			// The rom needs to be visible in the 0x4000-0x7FFF range
			// However in an NMS8250 the FDC registers are read
			// from 0x4FF8-0x4FFF and 0xBFF8-0xBFFF
			break;
		}
		return value;
	} else {
		return romBank[address & 0x3FFF];
	}
}

void MSXFDC::writeIO(byte port, byte value, const EmuTime &time)
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

void MSXFDC::writeMem(word address, byte value, const EmuTime &time)
{
	//PRT_DEBUG("MSXFDC::writeMem(0x" << std::hex << (int)address << std::dec << ", value "<<(int)value<<").");
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
			};
			controller->setDriveSelect(drivenr, time);
			controller->setMotor((value & 128) ? 1 : 0, time); // set motor for current drive
			break;
	}
}

byte* MSXFDC::getReadCacheLine(word start)
{
	//if address overlap 0x7ff8-0x7ffb then return NULL, else normal ROM behaviour
	if ((start & 0x3FF8 & CPU::CACHE_LINE_HIGH) == (0x3FF8 & CPU::CACHE_LINE_HIGH))
		return NULL;
	if (start > 0x7FFF)
		return emptyRom;
	return &romBank[start & 0x3fff];
}
