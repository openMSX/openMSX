
#include "MSXMapperIO.hh"
#include "MSXMapperIOTurboR.hh"
#include "MSXMotherBoard.hh"


MSXMapperIO::MSXMapperIO()
{
}

MSXMapperIO::~MSXMapperIO()
{
}

MSXMapperIO *MSXMapperIO::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new MSXMapperIOTurboR();
		// TODO check config file for which specific mapperIO type 
	}
	return oneInstance;
}
MSXMapperIO *MSXMapperIO::oneInstance = NULL;


void MSXMapperIO::init()
{
	MSXDevice::init();
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

void MSXMapperIO::reset()
{
	//TODO mapper is initialized like this by BIOS,
	// but in what state is it after reset?
	pageNum[0] = 3;
	pageNum[1] = 2;
	pageNum[2] = 1;
	pageNum[3] = 0;
}

void MSXMapperIO::writeIO(byte port, byte value, Emutime &time)
{
	assert (0xfc <= port);
	pageNum[port-0xfc] = value;
}

//void MSXMapperIO::saveState(std::ofstream &writestream);
//void MSXMapperIO::restoreState(std::string &devicestring, std::ifstream &readstream);

byte MSXMapperIO::getPageNum(int page)
{
	assert(page <= 3);
	return pageNum[page];
}


//These methods are specific for each type of MSXMapperIO
//  byte MSXMapperIO::readIO(byte port, Emutime &time);
//  void MSXMapperIO::registerMapper(int blocks);

