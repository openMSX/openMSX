#include "MSXMapperIO.hh"
#include "MSXMotherBoard.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "enumerate.hh"
#include "outer.hh"
#include "serialize.hh"
#include "stl.hh"
#include <algorithm>

namespace openmsx {

[[nodiscard]] static byte calcReadBackMask(MSXMotherBoard& motherBoard)
{
	std::string_view type = motherBoard.getMachineConfig()->getConfig().getChildData(
	                               "MapperReadBackBits", "largest");
	if (type == "largest") {
		return 0x00; // all bits can be read
	}
	auto bits = StringOp::stringTo<int>(type);
	if (!bits) {
		throw FatalError("Unknown mapper type: \"", type, "\".");
	}
	if (*bits < 0 || *bits > 8) {
		throw FatalError("MapperReadBackBits out of range: \"", type, "\".");
	}
	return unsigned(-1) << *bits;
}

MSXMapperIO::MSXMapperIO(const DeviceConfig& config)
	: MSXDevice(config)
	, debuggable(getMotherBoard(), getName())
	, mask(calcReadBackMask(getMotherBoard()))
{
	reset(EmuTime::dummy());
}

void MSXMapperIO::registerMapper(MSXMemoryMapperInterface* mapper)
{
	mappers.push_back(mapper);
}

void MSXMapperIO::unregisterMapper(MSXMemoryMapperInterface* mapper)
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
	// Note: the mappers are responsible for invalidating/filling the CPU
	// cache-lines.
	for (auto* mapper : mappers) {
		mapper->writeIO(port, value, time);
	}
}


// SimpleDebuggable
// This debuggable is here for backwards compatibility. For more accurate
// results, use the debuggable belonging to a specific mapper.

MSXMapperIO::Debuggable::Debuggable(MSXMotherBoard& motherBoard_,
                                    const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_, "Memory mapper registers", 4)
{
}

byte MSXMapperIO::Debuggable::read(unsigned address)
{
	auto& mapperIO = OUTER(MSXMapperIO, debuggable);
	byte result = 0;
	for (auto* mapper : mapperIO.mappers) {
		result = std::max(result, mapper->getSelectedSegment(address));
	}
	return result;
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
		for (auto [page, reg] : enumerate(registers)) {
			writeIO(word(page), reg, EmuTime::dummy());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXMapperIO);

} // namespace openmsx
