// $Id$

#include "MSXFDC.hh"
#include "FDC2793.hh"
#include "CPU.hh"
#include "MSXMotherBoard.hh"


	//: MSXDevice(config, time), MSXMemDevice(config, time) , MSXRom16KB(config, time)
MSXFDC::MSXFDC(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXRom(config, time), MSXIODevice(config, time)
{
	PRT_DEBUG("Creating an MSXFDC object");
	
	// The loading of the diskrom and the mapping in the slot layout
	// has been done by the MSXRom
	
	brokenFDCread = false;
	emptyRom = new byte[CPU::CACHE_LINE_SIZE];
	memset(emptyRom, 255, CPU::CACHE_LINE_SIZE);
	
	std::string fdcChip = deviceConfig->getParameter("chip");
	PRT_DEBUG( "FDC of chiptype " << fdcChip );
	if (fdcChip == "2793")
		controller = new FDC2793(config);
	try {
		brokenFDCread = deviceConfig->getParameterAsBool("brokenFDCread");
		PRT_DEBUG( "brokenFDCread   " << brokenFDCread );
	} catch(MSXConfig::Exception& e) {
	}
	std::string interfaceType="hybrid";
	try {
		interfaceType= deviceConfig->getParameter("interface");
		PRT_DEBUG( "interfaceType   " << interfaceType );
	} catch(MSXConfig::Exception& e) {
		interfaceType="hybrid";
	}
	
	if (strcasecmp(interfaceType.c_str(),"memory")){
	  interface=0;
	};
	if (strcasecmp(interfaceType.c_str(),"port")){
	  interface=1;
	};
	if (strcasecmp(interfaceType.c_str(),"hybrid")){
	  interface=2;
	};
	
	if (interface){
	  //register extra IO ports for brazilian based interfaces
	  MSXMotherBoard::instance()->register_IO_In (0xD0,this);
	  MSXMotherBoard::instance()->register_IO_In (0xD1,this);
	  MSXMotherBoard::instance()->register_IO_In (0xD2,this);
	  MSXMotherBoard::instance()->register_IO_In (0xD3,this);
	};
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
	//if address overlap 0x7ff8-0x7ffb then return FDC , else normal ROM behaviour
	PRT_DEBUG("MSXFDC::readMem(0x" << std::hex << (int)address << std::dec << ").");
	if ( interface != 1 ){
	  // if hybrid or memory based
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
	      PRT_DEBUG("Got byte from disk : "<<(int)value);
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
	      value = controller->getDriveSelect(time);
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
	switch (port){
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
	  	// From the source of brMSX all I could find out was 
		// bit 4 is used for side select
		// bit 0 influences the led settings
		driveD4=value;
		if (value&16){
		  controller->setSideSelect(1, time);
		} else {
		  controller->setSideSelect(0, time);
		};
		// Since these two bits are all that I know of right now I will
		// assume diska and map the led stuff into motor on/off states
	  	if (value&1){
		  controller->setDriveSelect(128, time);
		} else {
		  controller->setDriveSelect(0, time);
		}
	    break;
	};
}

void MSXFDC::writeMem(word address, byte value, const EmuTime &time)
{
	//PRT_DEBUG("MSXFDC::writeMem(0x" << std::hex << (int)address << std::dec << ", value "<<(int)value<<").");
	switch (address & 0x3FFF){
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
		controller->setSideSelect(value, time);
		break;
	case 0x3FFD:
		//bit 1,0 -> drive number  (00 or 10: drive A, 01: drive B, 11: nothing)
		//bit 7 -> motor on
		//TODO check other bits !!
		controller->setDriveSelect(value, time);
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
