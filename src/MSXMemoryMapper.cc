// $Id$

#include <string>
#include "MSXMemoryMapper.hh"
#include "MSXMapperIO.hh"
#include "MSXMotherBoard.hh"
#include "MSXMapperIOTurboR.hh"
#include "MSXMapperIOPhilips.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"


MSXMemoryMapper::MSXMemoryMapper(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXMemoryMapper object");
	
	initIO();
	
	slowDrainOnReset = deviceConfig->getParameterAsBool("slow_drain_on_reset");
	int kSize = deviceConfig->getParameterAsInt("size");
	blocks = kSize/16;
	int size = 16384*blocks;
	if (!(buffer = new byte[size]))
		PRT_ERROR("Couldn't allocate memory for " << getName());
	//Isn't completely true, but let's suppose that ram will
	//always contain all zero if started
	memset(buffer, 0, size);
	 
	device->registerMapper(blocks);
}

MSXMemoryMapper::~MSXMemoryMapper()
{
	PRT_DEBUG("Destructing an MSXMemoryMapper object");
	delete [] buffer; // C++ can handle null-pointers
}

void MSXMemoryMapper::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
	resetIO();
	if (!slowDrainOnReset) {
		PRT_DEBUG("Clearing ram of " << getName());
		memset(buffer, 0, blocks*16384);
	}
}

byte MSXMemoryMapper::readMem(word address, const EmuTime &time)
{
	return buffer[getAdr(address)];
}

void MSXMemoryMapper::writeMem(word address, byte value, const EmuTime &time)
{
	buffer[getAdr(address)] = value;
}

byte* MSXMemoryMapper::getReadCacheLine(word start, word length)
{
	return &buffer[getAdr(start)];
}

byte* MSXMemoryMapper::getWriteCacheLine(word start, word length)
{
	return &buffer[getAdr(start)];
}

word MSXMemoryMapper::getAdr(word address)
{
	byte page = pageNum[address>>14] % blocks;
	return ((page<<14) | (address&0x3fff));
}



void MSXMemoryMapper::initIO()
{
	if (device == NULL) {
		// Create specified IO behaviour
		MSXConfig::Config* config = MSXConfig::instance()->getConfigById("MapperIO");
		string type = config->getParameter("type");
		if (type == "TurboR")
			device = new MSXMapperIOTurboR();
		else if (type == "Philips")
			device = new MSXMapperIOPhilips();
		else
			PRT_ERROR("Unknown mapper type");
		
		// Register I/O ports FC..FF 
		MSXMotherBoard::instance()->register_IO_In (0xFC,this);
		MSXMotherBoard::instance()->register_IO_In (0xFD,this);
		MSXMotherBoard::instance()->register_IO_In (0xFE,this);
		MSXMotherBoard::instance()->register_IO_In (0xFF,this);
		MSXMotherBoard::instance()->register_IO_Out(0xFC,this);
		MSXMotherBoard::instance()->register_IO_Out(0xFD,this);
		MSXMotherBoard::instance()->register_IO_Out(0xFE,this);
		MSXMotherBoard::instance()->register_IO_Out(0xFF,this);
		
		resetIO();
	}
}
MSXMapperIO *MSXMemoryMapper::device = NULL;

void MSXMemoryMapper::resetIO()
{
	//TODO mapper is initialized like this by BIOS,
	// but in what state is it after reset?
	pageNum[0] = 3;
	pageNum[1] = 2;
	pageNum[2] = 1;
	pageNum[3] = 0;
}

byte MSXMemoryMapper::readIO(byte port, const EmuTime &time)
{
	assert(0xfc <= port);
	return device->convert(pageNum[port-0xfc]);
}

void MSXMemoryMapper::writeIO(byte port, byte value, const EmuTime &time)
{
	assert (0xfc <= port);
	pageNum[port-0xfc] = value;
	MSXCPU::instance()->invalidateCache(0x4000*(port-0xfc), 0x4000);
}

byte MSXMemoryMapper::pageNum[4];

