// $Id$

#include "MSXMapperIO.hh"
#include "MSXMapperIOTurboR.hh"
#include "MSXMapperIOPhilips.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include "MSXConfig.hh"
#include "Config.hh"
#include "Device.hh"

namespace openmsx {

MSXMapperIO::MSXMapperIO(Device* device, const EmuTime& time)
	: MSXDevice(device, time), MSXIODevice(device, time)
{
	Config* config = MSXConfig::instance()->findConfigById("MapperType");
	if (config) {
		const string& type = config->getParameter("type");
		if (type == "TurboR") {
			mapperMask = new MSXMapperIOTurboR();
		} else if (type == "Philips") {
			mapperMask = new MSXMapperIOPhilips();
		} else {
			throw FatalError("Unknown mapper type");
		}
	} else {
		mapperMask = new MSXMapperIOPhilips();
	}
	mask = mapperMask->calcMask(mapperSizes);

	reset(time);
}

MSXMapperIO::~MSXMapperIO()
{
	delete mapperMask;
}

void MSXMapperIO::registerMapper(unsigned blocks)
{
	mapperSizes.insert(blocks);
	mask = mapperMask->calcMask(mapperSizes);
}

void MSXMapperIO::unregisterMapper(unsigned blocks)
{
	mapperSizes.erase(mapperSizes.find(blocks));
	mask = mapperMask->calcMask(mapperSizes);
}


void MSXMapperIO::reset(const EmuTime& time)
{
	//TODO in what state is mapper after reset?
	// Zeroed is most likely.
	// To find out for real, insert an external memory mapper on an MSX1.
	for (unsigned i = 0; i < 4; ++i) {
		page[i] = 0;
	}
}

byte MSXMapperIO::readIO(byte port, const EmuTime& time)
{
	return page[port & 0x03] | mask;
}

void MSXMapperIO::writeIO(byte port, byte value, const EmuTime& time)
{
	port &= 0x03;
	page[port] = value;
	MSXCPU::instance()->invalidateCache(
		0x4000 * port, 0x4000 / CPU::CACHE_LINE_SIZE );
}

byte MSXMapperIO::getSelectedPage(byte bank)
{
	return page[bank];
}

} // namespace openmsx

