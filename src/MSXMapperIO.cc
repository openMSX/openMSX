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
}

MSXMapperIO* MSXMapperIO::instance()
{
	if (oneInstance == NULL) {
		//std::list<MSXConfig::Device*> deviceList;
		MSXConfig::Device* config = MSXConfig::Backend::instance()->getDeviceById("MapperIO");
		//if (deviceList.size() != 1)
		//	PRT_ERROR("There must be exactly one MapperIO in config file");
		//MSXConfig::Device* config = deviceList.front();
		EmuTime dummy;
		std::string type = config->getParameter("type");
		if (type == "TurboR") {
			oneInstance = new MSXMapperIOTurboR(config, dummy);
		} else if (type == "Philips") {
			oneInstance = new MSXMapperIOPhilips(config, dummy);
		} else {
			PRT_ERROR("Unknown mapper type");
		}
	}
	return oneInstance;
}
MSXMapperIO* MSXMapperIO::oneInstance = NULL;


void MSXMapperIO::reset(const EmuTime &time)
{
	//TODO in what state is mapper after reset?
	// Zeroed is most likely.
	// To find out for real, insert an external memory mapper on an MSX1.
	for (int i = 0; i < 4; i++) pageAddr[i] = 0;
}

byte MSXMapperIO::readIO(byte port, const EmuTime &time)
{
	assert(0xfc <= port);
	return convert(pageAddr[port-0xfc] >> 14);
}

void MSXMapperIO::writeIO(byte port, byte value, const EmuTime &time)
{
	assert (0xfc <= port);
	pageAddr[port-0xfc] = value << 14;
	MSXCPU::instance()->invalidateCache(0x4000*(port-0xfc), 0x4000/CPU::CACHE_LINE_SIZE);
}

int MSXMapperIO::pageAddr[4];

