#include "MSXFDC.hh"
#include "FDC2793.hh"
#include "CPU.hh"


	//: MSXDevice(config, time), MSXMemDevice(config, time) , MSXRom16KB(config, time)
MSXFDC::MSXFDC(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXRom(config, time)
{
	PRT_DEBUG("Creating an MSXFDC object");
	brokenFDCread=false;
	emptyRom=NULL;
	emptyRom=new byte[CPU::CACHE_LINE_SIZE];
	if (emptyRom) {
		memset(emptyRom,255,CPU::CACHE_LINE_SIZE);
	}
	// The loading of the diskrom and the mapping in the slot layout has been done by the MSXRom
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
	delete emptyRom;
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
	byte value=255;
	//if address overlap 0x7ff8-0x7ffb then return FDC , else normal ROM behaviour
	PRT_DEBUG("MSXFDC::readMem(0x" << std::hex << (int)address << std::dec << ").");
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
	  PRT_DEBUG("Got byte from disk : "<<(int)value);
	  break;
	case 0x3FFC:
	   //bit 0 = side select
	   //TODO check other bits !!
	   value=controller->getSideSelect(time);
	  break;
	case 0x3FFD:
	   //bit 1,0 -> drive number  (00 or 10: drive A, 01: drive B, 11: nothing)
	   //bit 7 -> motor on
	   //TODO check other bits !!
	   value=controller->getDriveSelect(time);
	  break;
	case 0x3FFE:
	   //not used
	   value=255;
	  break;
	case 0x3FFF:
	   // Drive control IRQ and DRQ lines are not connected to Z80 interrupt request
	   // bit 6: !intrq
	   // bit 7: !dtrq
	   //TODO check other bits !!
	   value=0xC0;
	   if (controller->getIRQ(time)) value&=0xBF ;
	   if (controller->getDTRQ(time)) value&=0x7F ;
	  break;
	   
	default:
	  if (address<0x8000)
	    value=romBank [address & 0x3FFF];
	  // quick hack to have FDC register in the correct ranges but not the rom
	  // (other wise calculus says to litle TPA memory :-)
	  // The rom needs to be visible in the 0x4000-0x7FFF range
	  // However in an NMS8250 the FDC registers are read
	  // from 0x4FF8-0x4FFF and 0xBFF8-0xBFFF
	  break;
	}
	return value;
}

void MSXFDC::writeMem(word address, byte value, const EmuTime &time)
{
	//PRT_DEBUG("MSXFDC::writeMem(0x" << std::hex << (int)address << std::dec << ", value "<<(int)value<<").");
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
	  case 0x3FFC:
	    //bit 0 = side select
	    //TODO check other bits !!
	    controller->setSideSelect(value,time);
	    break;
	  case 0x3FFD:
	    //bit 1,0 -> drive number  (00 or 10: drive A, 01: drive B, 11: nothing)
	    //bit 7 -> motor on
	    //TODO check other bits !!
	    controller->setDriveSelect(value,time);
	    break;
	}
}

byte* MSXFDC::getReadCacheLine(word start)
{
	//if address overlap 0x7ff8-0x7ffb then return NULL, else normal ROM behaviour
	if ( (start&0x3FF8&CPU::CACHE_LINE_HIGH)==(0x3FF8&CPU::CACHE_LINE_HIGH) ) return NULL;
	if (start>0x7FFF) return emptyRom;
	return &romBank[start & 0x3fff];
	PRT_DEBUG("MSXFDC getReadCacheLine");
	return NULL;
}
byte* MSXFDC::getWriteCacheLine(word start)
{
	return NULL;
}
