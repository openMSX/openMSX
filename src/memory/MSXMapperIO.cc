// $Id$

#include "MSXMapperIO.hh"
#include "MSXMotherBoard.hh"
#include "SimpleDebuggable.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "serialize.hh"

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


static byte calcEngineMask(MSXMotherBoard& motherBoard)
{
	string type = motherBoard.getMachineConfig()->getConfig().getChildData(
	                               "MapperReadBackBits", "largest");
	if (type == "5") {
		return 0xE0; // upper 3 bits always read "1"
	} else if (type == "largest") {
		return 0x00; // all bits can be read
	} else {
		throw FatalError("Unknown mapper type: \"" + type + "\".");
	}
}

MSXMapperIO::MSXMapperIO(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, debuggable(new MapperIODebuggable(motherBoard, *this))
	, engineMask(calcEngineMask(motherBoard))
{
	updateMask();
	reset(EmuTime::dummy());
}

MSXMapperIO::~MSXMapperIO()
{
}

void MSXMapperIO::updateMask()
{
	// unused bits always read "1"
	unsigned largest = (mapperSizes.empty()) ? 1 : *mapperSizes.rbegin();
	mask = ((256 - Math::powerOfTwo(largest)) & 255) | engineMask;
}

void MSXMapperIO::registerMapper(unsigned blocks)
{
	mapperSizes.insert(blocks);
	updateMask();
}

void MSXMapperIO::unregisterMapper(unsigned blocks)
{
	mapperSizes.erase(mapperSizes.find(blocks));
	updateMask();
}

void MSXMapperIO::reset(EmuTime::param /*time*/)
{
	// TODO in what state is mapper after reset?
	// Zeroed is most likely.
	// To find out for real, insert an external memory mapper on an MSX1.
	for (unsigned i = 0; i < 4; ++i) {
		registers[i] = 0;
	}
}

byte MSXMapperIO::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MSXMapperIO::peekIO(word port, EmuTime::param /*time*/) const
{
	return getSelectedPage(port & 0x03) | mask;
}

void MSXMapperIO::writeIO(word port, byte value, EmuTime::param /*time*/)
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
	invalidateMemCache(0x4000 * address, 0x4000);
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


template<typename Archive>
void MSXMapperIO::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("registers", registers);
	// all other state is reconstructed in another way
}
INSTANTIATE_SERIALIZE_METHODS(MSXMapperIO);

} // namespace openmsx
