// $Id$

#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"
#include "MSXMotherBoard.hh"


MSXMemoryMapper::MSXMemoryMapper()
{
	PRT_DEBUG("Creating an MSXMemoryMapper object");
}

MSXMemoryMapper::~MSXMemoryMapper()
{
	PRT_DEBUG("Destructing an MSXMemoryMapper object");
	delete [] buffer; // C++ can handle null-pointers
}

void MSXMemoryMapper::init()
{
	MSXDevice::init();
	
	if (deviceConfig->getParameter("slow_drain_on_reset") == "true")
		slow_drain_on_reset = true;
	else	slow_drain_on_reset = false;
	
	int kSize = atoi(deviceConfig->getParameter("size").c_str());
	blocks = kSize/16;
	int size = 16384*blocks;
	buffer = new byte[size];
	if (buffer == NULL)
		PRT_ERROR("Couldn't allocate memory for " << getName());
	//Isn't completely true, but let's suppose that ram will
	//always contain all zero if started
	memset(buffer, 0, size);
	 
	std::list<MSXConfig::Device::Slotted*> slotted_list = deviceConfig->slotted;
	for (std::list<MSXConfig::Device::Slotted*>::const_iterator i=slotted_list.begin(); i != slotted_list.end(); i++) {
		MSXMotherBoard::instance()->registerSlottedDevice(this, (*i)->getPS(),
		                                                        (*i)->getSS(),
		                                                        (*i)->getPage() );
	}
	
	MSXMapperIO::instance()->registerMapper(blocks);
}

void MSXMemoryMapper::reset()
{
	MSXDevice::reset();
	if (!slow_drain_on_reset) {
		PRT_DEBUG("Clearing ram of " << getName());
		memset(buffer, 0, blocks*16384);
	}
}

byte MSXMemoryMapper::readMem(word address, Emutime &time)
{
	return buffer[getAdr(address)];
}

void MSXMemoryMapper::writeMem(word address, byte value, Emutime &time)
{
	buffer[getAdr(address)] = value;
}

int MSXMemoryMapper::getAdr(int address)
{
	int pageNum = MSXMapperIO::instance()->getPageNum(address>>14);
	pageNum %= blocks;
	int adr = pageNum*16384 + address&0x3fff;
	return adr;
}

//void MSXMemoryMapper::saveState(std::ofstream &writestream);
//void MSXMemoryMapper::restoreState(std::string &devicestring, std::ifstream &readstream);
