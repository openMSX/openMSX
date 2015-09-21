#include "MSXMapperIO.hh"
#include "MSXMotherBoard.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "outer.hh"
#include "serialize.hh"
#include "stl.hh"

namespace openmsx {

static byte calcEngineMask(MSXMotherBoard& motherBoard)
{
	string_ref type = motherBoard.getMachineConfig()->getConfig().getChildData(
	                               "MapperReadBackBits", "largest");
	if (type == "5") {
		return 0xE0; // upper 3 bits always read "1"
	} else if (type == "largest") {
		return 0x00; // all bits can be read
	} else {
		throw FatalError("Unknown mapper type: \"" + type + "\".");
	}
}

MSXMapperIO::MSXMapperIO(const DeviceConfig& config)
	: MSXDevice(config)
	, debuggable(getMotherBoard(), getName())
	, engineMask(calcEngineMask(getMotherBoard()))
{
	updateMask();
	reset(EmuTime::dummy());
}

void MSXMapperIO::updateMask()
{
	// unused bits always read "1"
	unsigned largest = (mapperSizes.empty()) ? 1 : mapperSizes.back();
	mask = ((256 - Math::powerOfTwo(largest)) & 255) | engineMask;
}

void MSXMapperIO::registerMapper(unsigned blocks)
{
	auto it = upper_bound(begin(mapperSizes), end(mapperSizes), blocks);
	mapperSizes.insert(it, blocks);
	updateMask();
}

void MSXMapperIO::unregisterMapper(unsigned blocks)
{
	mapperSizes.erase(rfind_unguarded(mapperSizes, blocks));
	updateMask();
}

void MSXMapperIO::reset(EmuTime::param /*time*/)
{
	// TODO in what state is mapper after reset?
	// Zeroed is most likely.
	// To find out for real, insert an external memory mapper on an MSX1.
	for (auto& reg : registers) {
		reg = 0;
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

void MSXMapperIO::write(unsigned address, byte value)
{
	registers[address] = value;
	invalidateMemCache(0x4000 * address, 0x4000);
}


// SimpleDebuggable

MSXMapperIO::Debuggable::Debuggable(MSXMotherBoard& motherBoard_,
                                    const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_, "Memory mapper registers", 4)
{
}

byte MSXMapperIO::Debuggable::read(unsigned address)
{
	auto& mapperIO = OUTER(MSXMapperIO, debuggable);
	return mapperIO.getSelectedPage(address);
}

void MSXMapperIO::Debuggable::write(unsigned address, byte value)
{
	auto& mapperIO = OUTER(MSXMapperIO, debuggable);
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
