// $Id$

#include <string>
#include "MSXMapperIO.hh"
#include "MSXMapperIOTurboR.hh"
#include "MSXMapperIOPhilips.hh"
#include "MSXMotherBoard.hh"


MSXMapperIO::MSXMapperIO(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	oneInstance = this;
	
	// Create specified IO behaviour
	string type = deviceConfig->getParameter("type");
	if (type == "TurboR")
		device = new MSXMapperIOTurboR();
	else if (type == "Philips")
		device = new MSXMapperIOPhilips();
	else
		PRT_ERROR("Unknown mapper type");
	
	// Register I/O ports FC..FF for reading
	MSXMotherBoard::instance()->register_IO_In (0xFC,this);
	MSXMotherBoard::instance()->register_IO_In (0xFD,this);
	MSXMotherBoard::instance()->register_IO_In (0xFE,this);
	MSXMotherBoard::instance()->register_IO_In (0xFF,this);
	// Register I/O ports FC..FF for writing
	MSXMotherBoard::instance()->register_IO_Out(0xFC,this);
	MSXMotherBoard::instance()->register_IO_Out(0xFD,this);
	MSXMotherBoard::instance()->register_IO_Out(0xFE,this);
	MSXMotherBoard::instance()->register_IO_Out(0xFF,this);
}

MSXMapperIO::~MSXMapperIO()
{
}

MSXMapperIO *MSXMapperIO::instance()
{
	if (oneInstance == NULL) {
		std::list<MSXConfig::Device*> deviceList;
		deviceList = MSXConfig::instance()->getDeviceByType("MapperIO");
		if (deviceList.size() != 1)
			PRT_ERROR("There must be exactly one MapperIO in config file");
		MSXConfig::Device* config = deviceList.front();
		EmuTime zero;
		new MSXMapperIO(config, zero);
	}
	return oneInstance;
}
MSXMapperIO *MSXMapperIO::oneInstance = NULL;


void MSXMapperIO::reset(const EmuTime &time)
{
	//TODO mapper is initialized like this by BIOS,
	// but in what state is it after reset?
	pageNum[0] = 3;
	pageNum[1] = 2;
	pageNum[2] = 1;
	pageNum[3] = 0;
}

byte MSXMapperIO::readIO(byte port, EmuTime &time)
{
	assert(0xfc <= port);
	return device->convert(pageNum[port-0xfc]);
}

void MSXMapperIO::writeIO(byte port, byte value, EmuTime &time)
{
	assert (0xfc <= port);
	pageNum[port-0xfc] = value;
}

byte MSXMapperIO::getPageNum(int page)
{
	assert(page <= 3);
	return pageNum[page];
}

void MSXMapperIO::registerMapper(int blocks)
{
	device->registerMapper(blocks);
}

