// $Id$

#include "MSXMapperIO.hh"
#include "MSXMapperIOTurboR.hh"
#include "MSXMapperIOPhilips.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include "MSXConfig.hh"


namespace openmsx {

MSXMapperIO::MSXMapperIO(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	string type = config->getParameter("type");
	if (type == "TurboR") {
		mapperMask = new MSXMapperIOTurboR();
	} else if (type == "Philips") {
		mapperMask = new MSXMapperIOPhilips();
	} else {
		throw FatalError("Unknown mapper type");
	}
	mask = mapperMask->calcMask(mapperSizes);

	reset(time);
}

MSXMapperIO::MSXMapperIO()
	// TODO use empty config iso NULL
	: MSXDevice(NULL, EmuTime::zero), MSXIODevice(NULL, EmuTime::zero)
{
	mapperMask = new MSXMapperIOPhilips();
	mask = mapperMask->calcMask(mapperSizes);
	reset(EmuTime::zero);
}

MSXMapperIO::~MSXMapperIO()
{
	delete mapperMask;
}

MSXMapperIO* MSXMapperIO::instance()
{
	static MSXMapperIO* oneInstance = NULL;
	if (oneInstance == NULL) {
		try {
			Device* config = MSXConfig::instance()->
				getDeviceById("MapperIO");
			oneInstance = new MSXMapperIO(config, EmuTime::zero);
		} catch (ConfigException &e) {
			// no MapperIO section, take default mapper type
			oneInstance = new MSXMapperIO();
		}
	}
	return oneInstance;
}


void MSXMapperIO::registerMapper(int blocks)
{
	mapperSizes.push_back(blocks);
	mask = mapperMask->calcMask(mapperSizes);
}

void MSXMapperIO::unregisterMapper(int blocks)
{
	list<int>::iterator i;
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
	for (int i=0; i<4; i++) page[i] = 0;
}

byte MSXMapperIO::readIO(byte port, const EmuTime &time)
{
	return page[port & 0x03] | mask;
}

void MSXMapperIO::writeIO(byte port, byte value, const EmuTime &time)
{
	port &= 0x03;
	page[port] = value;
	MSXCPU::instance()->invalidateCache(
		0x4000 * port, 0x4000 / CPU::CACHE_LINE_SIZE );
}

byte MSXMapperIO::getSelectedPage(int bank)
{
	return page[bank];
}

} // namespace openmsx

