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

static byte calcReadBackMask(MSXMotherBoard& motherBoard)
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
	, mask(calcReadBackMask(getMotherBoard()))
{
	reset(EmuTime::dummy());
}

void MSXMapperIO::registerMapper(MSXMemoryMapper* mapper)
{
	mappers.push_back(mapper);
}

void MSXMapperIO::unregisterMapper(MSXMemoryMapper* mapper)
{
	mappers.erase(rfind_unguarded(mappers, mapper));
}

byte MSXMapperIO::readIO(word port, EmuTime::param time)
{
	byte result = 0xFF;
	for (auto* mapper : mappers) {
		result &= mapper->readIO(port, time);
	}
	return result | mask;
}

byte MSXMapperIO::peekIO(word port, EmuTime::param time) const
{
	byte result = 0xFF;
	for (auto* mapper : mappers) {
		result &= mapper->peekIO(port, time);
	}
	return result | mask;
}

void MSXMapperIO::writeIO(word port, byte value, EmuTime::param time)
{
	for (auto* mapper : mappers) {
		mapper->writeIO(port, value, time);
	}
	invalidateMemCache(0x4000 * (port & 0x03), 0x4000);
}


// SimpleDebuggable

MSXMapperIO::Debuggable::Debuggable(MSXMotherBoard& motherBoard_,
                                    const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_, "Memory mapper registers", 4)
{
}

byte MSXMapperIO::Debuggable::read(unsigned address, EmuTime::param time)
{
	auto& mapperIO = OUTER(MSXMapperIO, debuggable);
	return mapperIO.peekIO(address, time);
}

void MSXMapperIO::Debuggable::write(unsigned address, byte value,
                                    EmuTime::param time)
{
	auto& mapperIO = OUTER(MSXMapperIO, debuggable);
	mapperIO.writeIO(address, value, time);
}


template<typename Archive>
void MSXMapperIO::serialize(Archive& ar, unsigned version)
{
	if (ar.versionBelow(version, 2)) {
		// In version 1 we stored the mapper state in MSXMapperIO instead of
		// in the individual mappers, so distribute the state to them.
		assert(ar.isLoader());
		byte registers[4];
		ar.serialize("registers", registers);
		for (int page = 0; page < 4; page++) {
			writeIO(page, registers[page], EmuTime::dummy());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXMapperIO);

} // namespace openmsx
