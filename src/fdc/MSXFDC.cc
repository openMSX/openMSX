#include "MSXFDC.hh"
#include "FDC2793.hh"


	//: MSXDevice(config, time), MSXMemDevice(config, time) , MSXRom16KB(config, time)
MSXFDC::MSXFDC(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXRom16KB(config, time)
{
	PRT_DEBUG("Creating an MSXFDC object");
	brokenFDCread=false;
	// The loading of the diskrom and the mapping in the slot layout has been done by the MSXRom16KB
	try {
		brokenFDCread = deviceConfig->getParameterAsBool("brokenFDCread");
		PRT_DEBUG( "brokenFDCread   " << brokenFDCread );
	} catch(MSXConfig::Exception& e) {
	}

	try {
		std::string fdcChip = deviceConfig->getParameter("chip");
		PRT_DEBUG( "FDC of chiptype " << fdcChip );
		if ( strcmp(fdcChip.c_str(),"2793") == 0 ) {
		  controller=new FDC2793(config);
		}
	} catch(MSXConfig::Exception& e) {
		// No chip type found
	}

}

MSXFDC::~MSXFDC()
{
	delete controller;
	PRT_DEBUG("Destructing an MSXFDC object");
}

void MSXFDC::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
	if (controller) controller->reset();
}

byte MSXFDC::readMem(word address, const EmuTime &time)
{
	byte value;
	//if address overlap 0x7ff8-0x7ffb then return FDC , else normal ROM behaviour
	PRT_DEBUG("MSXFDC::readMem(0x" << std::hex << (int)address << ").");
	switch (address & 0x3FFF){
	case 0x3FF8:
	  value=controller->getStatusReg(time);
	  break;
	case 0x3FF9:
	  value=brokenFDCread ? 255 : controller->getTrackReg(time);
	  //TODO: check if such broken interfaces indeed return 255 or something else
	  // example of such machines : Sony 700 series
	  break;
	case 0x3FFA:
	  value=brokenFDCread ? 255 : controller->getSectorReg(time);
	  //TODO: check if such broken interfaces indeed return 255 or something else
	  break;
	case 0x3FFB:
	  value=controller->getDataReg(time);
	  break;
	case 0x3FFC:
	   //bit 0 = side select
	   value=0;
	  break;
	case 0x3FFD:
	   //bit 1,0 -> drive number  (00 or 10: drive A, 01: drive B, 11: nothing)
	   //bit 7 -> motor on
	   value=0;
	  break;
	case 0x3FFE:
	   //not used
	   value=255;
	  break;
	case 0x3FFF:
	   // Drive control IRQ and DRQ lines are not connected to Z80 interrupt request
	   // bit 6: !intrq
	   // bit 7: !dtrq
	   value=0;
	  break;
	   
	default:
	  value=memoryBank [address & 0x3FFF];
	  break;
	}
	return value;
}

void MSXFDC::writeMem(word address, byte value, const EmuTime &time)
{
	PRT_DEBUG("MSXFDC::writeMem(0x" << std::hex << (int)address << std::dec
		<< ", value "<<(int)value<<").");
	switch (address & 0x3FFF){
	  case 0x3FF8:
	    controller->setCommandReg(value,time);
	    break;
	  case 0x3FF9:
	    controller->setTrackReg(value,time);
	    break;
	  case 0x3FFA:
	    controller->setSectorReg(value,time);
	    break;
	  case 0x3FFB:
	    controller->setDataReg(value,time);
	    break;
	}
}

byte* MSXFDC::getReadCacheLine(word start)
{
	//if address overlap 0x7ff8-0x7ffb then return NULL, else normal ROM behaviour
	if ( (start&0x3F00)==0x3F00 ) return NULL;
	return &memoryBank[start & 0x3fff];
}

