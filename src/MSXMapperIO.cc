// $Id$

#include "MSXMapperIO.hh"
#include "MSXMotherBoard.hh"
#include "MSXMapperIOTurboR.hh"
#include "MSXMapperIOPhilips.hh"
#include "MSXCPU.hh"
#include "CPU.hh"


MSXMapperIO::MSXMapperIO(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	PRT_DEBUG("Creating an MSXMapperIO object");

	std::string type = config->getParameter("type");
	if (type == "TurboR") {
		mapperMask = new MSXMapperIOTurboR();
	} else if (type == "Philips") {
		mapperMask = new MSXMapperIOPhilips();
	} else {
		PRT_ERROR("Unknown mapper type");
	}
	mask = mapperMask->calcMask(mapperSizes);
	
	// Register I/O ports FC..FF
	MSXMotherBoard::instance()->register_IO_In (0xFC,this);
	MSXMotherBoard::instance()->register_IO_In (0xFD,this);
	MSXMotherBoard::instance()->register_IO_In (0xFE,this);
	MSXMotherBoard::instance()->register_IO_In (0xFF,this);
	MSXMotherBoard::instance()->register_IO_Out(0xFC,this);
	MSXMotherBoard::instance()->register_IO_Out(0xFD,this);
	MSXMotherBoard::instance()->register_IO_Out(0xFE,this);
	MSXMotherBoard::instance()->register_IO_Out(0xFF,this);

	reset(time);
}

MSXMapperIO::~MSXMapperIO()
{
	PRT_DEBUG("Destroying an MSXMapperIO object");

	delete mapperMask;
}

MSXMapperIO* MSXMapperIO::instance()
{
	if (oneInstance == NULL) {
		MSXConfig::Device* config = MSXConfig::Backend::instance()->getDeviceById("MapperIO");
		EmuTime dummy;
		oneInstance = new MSXMapperIO(config, dummy);
	}
	return oneInstance;
}
MSXMapperIO* MSXMapperIO::oneInstance = NULL;


void MSXMapperIO::registerMapper(int blocks)
{
	mapperSizes.push_back(blocks);
	mask = mapperMask->calcMask(mapperSizes);
}

void MSXMapperIO::unregisterMapper(int blocks)
{
	std::list<int>::iterator i;
	for (i=mapperSizes.begin(); i!=mapperSizes.end(); i++) {
		if (*i == blocks) {
			mapperSizes.erase(i);
			break;
		}
	}
	mask = mapperMask->calcMask(mapperSizes);
}


void MSXMapperIO::reset(const EmuTime &time)
{
	//TODO in what state is mapper after reset?
	// Zeroed is most likely.
	// To find out for real, insert an external memory mapper on an MSX1.
	for (int i=0; i<4; i++)
		page[i] = 0;
}

byte MSXMapperIO::readIO(byte port, const EmuTime &time)
{
	assert(0xfc <= port);
	return page[port-0xfc] | mask;
}

void MSXMapperIO::writeIO(byte port, byte value, const EmuTime &time)
{
	assert (0xfc <= port);
	page[port-0xfc] = value;
	MSXCPU::instance()->invalidateCache(0x4000*(port-0xfc), 0x4000/CPU::CACHE_LINE_SIZE);
}


byte MSXMapperIO::getSelectedPage(int bank)
{
	return page[bank];
}

