// $Id$

#include "MSXMapperIO.hh"
#include "MSXMapperIOTurboR.hh"
#include "MSXMapperIOPhilips.hh"
#include "MSXCPU.hh"
#include "HardwareConfig.hh"

using std::string;

namespace openmsx {

MSXMapperIO::MSXMapperIO(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
{
	string type = HardwareConfig::instance().
		getChildData("MapperReadBackBits", "largest");
	if (type == "5") {
		mapperMask.reset(new MSXMapperIOTurboR());
	} else if (type == "largest") {
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

byte MSXMapperIO::readIO(byte port, const EmuTime& time)
{
	return peekIO(port, time);
}

byte MSXMapperIO::peekIO(byte port, const EmuTime& /*time*/) const
{
	return page[port & 0x03] | mask;
}

void MSXMapperIO::writeIO(byte port, byte value, const EmuTime& /*time*/)
{
	port &= 0x03;
	page[port] = value;
	MSXCPU::instance().invalidateMemCache(0x4000 * port, 0x4000);
}

byte MSXMapperIO::getSelectedPage(byte bank)
{
	return page[bank];
}

} // namespace openmsx

