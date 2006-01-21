// $Id$

#include "MSXMapperIO.hh"
#include "MSXMapperIOTurboR.hh"
#include "MSXMapperIOPhilips.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "SimpleDebuggable.hh"
#include "MachineConfig.hh"
#include "MSXException.hh"

using std::string;

namespace openmsx {

class MapperIODebuggable : public SimpleDebuggable
{
public:
	MapperIODebuggable(MSXMotherBoard& motherBoard, MSXMapperIO& mapperIO);
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);
private:
	MSXMapperIO& mapperIO;
};


MSXMapperIO::MSXMapperIO(MSXMotherBoard& motherBoard, const XMLElement& config,
                         const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, debuggable(new MapperIODebuggable(motherBoard, *this))
{
	string type = motherBoard.getMachineConfig().getConfig().getChildData(
	                               "MapperReadBackBits", "largest");
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

byte MSXMapperIO::readIO(word port, const EmuTime& time)
{
	return peekIO(port, time);
}

byte MSXMapperIO::peekIO(word port, const EmuTime& /*time*/) const
{
	return getSelectedPage(port & 0x03) | mask;
}

void MSXMapperIO::writeIO(word port, byte value, const EmuTime& /*time*/)
{
	write(port & 0x03, value);
}

byte MSXMapperIO::getSelectedPage(byte bank) const
{
	return registers[bank];
}

void MSXMapperIO::write(unsigned address, byte value)
{
	registers[address] = value;
	getMotherBoard().getCPU().invalidateMemCache(0x4000 * address, 0x4000);
}


// SimpleDebuggable

MapperIODebuggable::MapperIODebuggable(MSXMotherBoard& motherBoard,
                                       MSXMapperIO& mapperIO_)
	: SimpleDebuggable(motherBoard, mapperIO_.getName(),
	                   "Memory mapper registers", 4)
	, mapperIO(mapperIO_)
{
}

byte MapperIODebuggable::read(unsigned address)
{
	return mapperIO.getSelectedPage(address);
}

void MapperIODebuggable::write(unsigned address, byte value)
{
	mapperIO.write(address, value);
}

} // namespace openmsx
