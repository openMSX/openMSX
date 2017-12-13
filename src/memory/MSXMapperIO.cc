#include "MSXMapperIO.hh"
#include "MSXMemoryMapper.hh"
#include "MSXMotherBoard.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "StringOp.hh"
#include "outer.hh"
#include "serialize.hh"
#include "stl.hh"
#include <algorithm>

namespace openmsx {

static byte calcEngineMask(MSXMotherBoard& motherBoard)
{
	string_ref type = motherBoard.getMachineConfig()->getConfig().getChildData(
	                               "MapperReadBackBits", "largest");
	if (type == "largest") {
		return 0x00; // all bits can be read
	}
	std::string str = type.str();
	int bits;
	if (!StringOp::stringToInt(str, bits)) {
		throw FatalError("Unknown mapper type: \"" + type + "\".");
	}
	if (bits < 0 || bits > 8) {
		throw FatalError("MapperReadBackBits out of range: \"" + type + "\".");
	}
	return -1 << bits;
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
	unsigned largest = 1;
	for (auto* mapper : mappers) {
		largest = std::max(largest, mapper->getSizeInBlocks());
	}
	// unused bits always read "1"
	mask = ((256 - Math::powerOfTwo(largest)) & 255) | engineMask;
}

void MSXMapperIO::registerMapper(MSXMemoryMapper* mapper)
{
	mappers.push_back(mapper);
	updateMask();
}

void MSXMapperIO::unregisterMapper(MSXMemoryMapper* mapper)
{
	mappers.erase(rfind_unguarded(mappers, mapper));
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
	return getSelectedSegment(port & 0x03) | mask;
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
	return mapperIO.getSelectedSegment(address);
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
