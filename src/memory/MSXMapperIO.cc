// $Id$

#include "MSXMapperIO.hh"
#include "MSXMapperIOTurboR.hh"
#include "MSXMapperIOPhilips.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include "HardwareConfig.hh"

namespace openmsx {

MSXMapperIO::MSXMapperIO(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
{
	string type = HardwareConfig::instance().
		getChildData("MapperType", "Philips");
	if (type == "TurboR") {
		mapperMask.reset(new MSXMapperIOTurboR());
	} else if (type == "Philips") {
		mapperMask.reset(new MSXMapperIOPhilips());
	} else {
		throw FatalError("Unknown mapper type: \"" + type + "\".");
	}
	mask = mapperMask->calcMask(mapperSizes);

	reset(time);
}

MSXMapperIO::~MSXMapperIO()
{
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


void MSXMapperIO::reset(const EmuTime& /*time*/)
{
	//TODO in what state is mapper after reset?
	// Zeroed is most likely.
	// To find out for real, insert an external memory mapper on an MSX1.
	for (unsigned i = 0; i < 4; ++i) {
		page[i] = 0;
	}
}

byte MSXMapperIO::readIO(byte port, const EmuTime& /*time*/)
{
	return page[port & 0x03] | mask;
}

void MSXMapperIO::writeIO(byte port, byte value, const EmuTime& /*time*/)
{
	port &= 0x03;
	page[port] = value;
	MSXCPU::instance().invalidateCache(
		0x4000 * port, 0x4000 / CPU::CACHE_LINE_SIZE );
}

byte MSXMapperIO::getSelectedPage(byte bank)
{
	return page[bank];
}

} // namespace openmsx

