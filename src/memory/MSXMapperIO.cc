// $Id$

#include "MSXMapperIO.hh"
#include "MSXMapperIOTurboR.hh"
#include "MSXMapperIOPhilips.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "HardwareConfig.hh"
#include "MSXException.hh"

using std::string;

namespace openmsx {

MSXMapperIO::MSXMapperIO(MSXMotherBoard& motherBoard, const XMLElement& config,
                         const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, SimpleDebuggable(motherBoard, getName(),
	                   "Memory mapper registers", 4)
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
		registers[i] = 0;
	}
}

byte MSXMapperIO::readIO(byte port, const EmuTime& time)
{
	return peekIO(port, time);
}

byte MSXMapperIO::peekIO(byte port, const EmuTime& /*time*/) const
{
	return getSelectedPage(port & 0x03) | mask;
}

void MSXMapperIO::writeIO(byte port, byte value, const EmuTime& /*time*/)
{
	write(port & 0x03, value);
}

byte MSXMapperIO::getSelectedPage(byte bank) const
{
	return registers[bank];
}


// SimpleDebuggable

byte MSXMapperIO::read(unsigned address)
{
	return getSelectedPage(address);
}

void MSXMapperIO::write(unsigned address, byte value)
{
	registers[address] = value;
	getMotherBoard().getCPU().invalidateMemCache(0x4000 * address, 0x4000);
}

} // namespace openmsx
