#include "MSXMapperIO.hh"

#include "MSXMotherBoard.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "MSXException.hh"
#include "serialize.hh"

#include "StringOp.hh"
#include "enumerate.hh"
#include "outer.hh"
#include "stl.hh"

#include <algorithm>

namespace openmsx {

[[nodiscard]] static byte calcReadBackMask(MSXMotherBoard& motherBoard)
{
	std::string_view type = motherBoard.getMachineConfig()->getConfig().getChildData(
	                               "MapperReadBackBits", "largest");
	if (type == "largest") {
		return 0xff; // all bits can be read
	}
	auto bits = StringOp::stringTo<int>(type);
	if (!bits) {
		throw FatalError("Unknown mapper type: \"", type, "\".");
	}
	if (*bits < 0 || *bits > 8) {
		throw FatalError("MapperReadBackBits out of range: \"", type, "\".");
	}
	return byte(~(unsigned(-1) << *bits));
}

MSXMapperIO::MSXMapperIO(const DeviceConfig& config)
	: MSXDevice(config)
	, debuggable(getMotherBoard(), getName())
	, mask(calcReadBackMask(getMotherBoard()))
{
	reset(EmuTime::dummy());
}

void MSXMapperIO::setMode(Mode mode_, byte mask_, byte baseValue_)
{
	mode = mode_;
	mask = mask_;
	baseValue = baseValue_;
}

void MSXMapperIO::registerMapper(MSXMemoryMapperInterface* mapper)
{
	mappers.push_back(mapper);
}

void MSXMapperIO::unregisterMapper(MSXMemoryMapperInterface* mapper)
{
	mappers.erase(rfind_unguarded(mappers, mapper));
}

byte MSXMapperIO::readIO(uint16_t port, EmuTime time)
{
	byte value = [&] {
		if (mode == Mode::EXTERNAL) {
			byte result = 0xFF;
			for (auto* mapper : mappers) {
				result &= mapper->readIO(port, time);
			}
			return result;
		} else {
			return registers[port & 3];
		}
	}();
	return (value & mask) | (baseValue & ~mask);
}

byte MSXMapperIO::peekIO(uint16_t port, EmuTime time) const
{
	byte value = [&] {
		if (mode == Mode::EXTERNAL) {
			byte result = 0xFF;
			for (const auto* mapper : mappers) {
				result &= mapper->peekIO(port, time);
			}
			return result;
		} else {
			return registers[port & 3];
		}
	}();
	return (value & mask) | (baseValue & ~mask);
}

void MSXMapperIO::writeIO(uint16_t port, byte value, EmuTime time)
{
	registers[port & 3] = value;

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
	// return the last written value, with full 8-bit precision.
	auto& mapperIO = OUTER(MSXMapperIO, debuggable);
	return mapperIO.registers[address & 3];
}

void MSXMapperIO::Debuggable::readBlock(unsigned start, std::span<byte> output)
{
	auto& mapperIO = OUTER(MSXMapperIO, debuggable);
	copy_to_range(std::span{mapperIO.registers}.subspan(start, output.size()), output);
}

void MSXMapperIO::Debuggable::write(unsigned address, byte value,
                                    EmuTime time)
{
	auto& mapperIO = OUTER(MSXMapperIO, debuggable);
	mapperIO.writeIO(narrow_cast<uint16_t>(address), value, time);
}


template<typename Archive>
void MSXMapperIO::serialize(Archive& ar, unsigned version)
{
	if (ar.versionBelow(version, 2)) {
		// In version 1 we stored the mapper state in MSXMapperIO instead of
		// in the individual mappers, so distribute the state to them.
		assert(Archive::IS_LOADER);
		ar.serialize("registers", registers);
		for (auto [page, reg] : enumerate(registers)) {
			writeIO(uint16_t(page), reg, EmuTime::dummy());
		}
	}
	if (ar.versionAtLeast(version, 3)) {
		// In version 3 we store the mapper state BOTH here and in the
		// individual mappers. The reason for this is:
		// - There are MSX machines with a S1985 but without a memory
		//   mapper. To support those we need to store the state here.
		// - In rare cases (e.g. musical memory mapper) the mapper state
		//   can be different for specific mappers.
		ar.serialize("registers", registers);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXMapperIO);

} // namespace openmsx
