#include "MSXMemoryMapperBase.hh"

#include "MSXException.hh"

#include "outer.hh"
#include "serialize.hh"

#include <algorithm>
#include <bit>

namespace openmsx {

[[nodiscard]] static unsigned getRamSize(const DeviceConfig& config)
{
	int kSize = config.getChildDataAsInt("size", 0);
	if ((kSize % 16) != 0) {
		throw MSXException("Mapper size is not a multiple of 16K: ", kSize);
	}
	if (kSize <= 0) {
		throw MSXException("Mapper size must be at least 16kB: ", kSize);
	}
	if (kSize > 4096) {
		throw MSXException("Mapper size must not be larger than 4096kB: ", kSize);
	}
	return kSize * 1024; // in bytes
}

MSXMemoryMapperBase::MSXMemoryMapperBase(const DeviceConfig& config)
	: MSXDevice(config)
	, MSXMapperIOClient(getMotherBoard())
	, checkedRam(config, getName(), "memory mapper", getRamSize(config))
	, debuggable(getMotherBoard(), getName())
{
}

unsigned MSXMemoryMapperBase::getBaseSizeAlignment() const
{
	return 0x4000;
}

void MSXMemoryMapperBase::powerUp(EmuTime time)
{
	checkedRam.clear();
	reset(time);
}

void MSXMemoryMapperBase::reset(EmuTime /*time*/)
{
	// Most mappers initialize to segment 0 for all pages.
	// On MSX2 and higher, the BIOS will select segments 3..0 for pages 0..3.
	std::ranges::fill(registers, 0);
}

byte MSXMemoryMapperBase::readIO(uint16_t port, EmuTime time)
{
	return peekIO(port, time);
}

byte MSXMemoryMapperBase::peekIO(uint16_t port, EmuTime /*time*/) const
{
	auto numSegments = narrow<unsigned>(checkedRam.size() / 0x4000);
	return registers[port & 0x03] | byte(~(std::bit_ceil(numSegments) - 1));
}

void MSXMemoryMapperBase::writeIOImpl(uint16_t port, byte value, EmuTime /*time*/)
{
	auto numSegments = narrow<unsigned>(checkedRam.size() / 0x4000);
	registers[port & 3] = value & byte(std::bit_ceil(numSegments) - 1);
	// Note: subclasses are responsible for handling CPU cacheline stuff
}

unsigned MSXMemoryMapperBase::segmentOffset(byte page) const
{
	unsigned segment = registers[page];
	auto numSegments = narrow<unsigned>(checkedRam.size() / 0x4000);
	segment = (segment < numSegments) ? segment : segment & (numSegments - 1);
	return segment * 0x4000;
}

unsigned MSXMemoryMapperBase::calcAddress(uint16_t address) const
{
	return segmentOffset(narrow<byte>(address / 0x4000)) | (address & 0x3fff);
}

byte MSXMemoryMapperBase::peekMem(uint16_t address, EmuTime /*time*/) const
{
	return checkedRam.peek(calcAddress(address));
}

byte MSXMemoryMapperBase::readMem(uint16_t address, EmuTime /*time*/)
{
	return checkedRam.read(calcAddress(address));
}

void MSXMemoryMapperBase::writeMem(uint16_t address, byte value, EmuTime /*time*/)
{
	checkedRam.write(calcAddress(address), value);
}

const byte* MSXMemoryMapperBase::getReadCacheLine(uint16_t start) const
{
	return checkedRam.getReadCacheLine(calcAddress(start));
}

byte* MSXMemoryMapperBase::getWriteCacheLine(uint16_t start)
{
	return checkedRam.getWriteCacheLine(calcAddress(start));
}


// SimpleDebuggable

MSXMemoryMapperBase::Debuggable::Debuggable(MSXMotherBoard& motherBoard_,
                                        const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " regs",
	                   "Memory mapper registers", 4)
{
}

byte MSXMemoryMapperBase::Debuggable::read(unsigned address)
{
	auto& mapper = OUTER(MSXMemoryMapperBase, debuggable);
	return mapper.registers[address];
}

void MSXMemoryMapperBase::Debuggable::readBlock(unsigned start, std::span<byte> output)
{
	auto& mapper = OUTER(MSXMemoryMapperBase, debuggable);
	copy_to_range(std::span{mapper.registers}.subspan(start, output.size()), output);
}

void MSXMemoryMapperBase::Debuggable::write(unsigned address, byte value)
{
	auto& mapper = OUTER(MSXMemoryMapperBase, debuggable);
	mapper.writeIO(narrow<uint16_t>(address), value, EmuTime::dummy());
}


template<typename Archive>
void MSXMemoryMapperBase::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("registers", registers);
	}
	// TODO ar.serialize("checkedRam", checkedRam);
	ar.serialize("ram", checkedRam.getUncheckedRam());
}
INSTANTIATE_SERIALIZE_METHODS(MSXMemoryMapperBase);
//REGISTER_MSXDEVICE(MSXMemoryMapperBase, "MemoryMapper");

} // namespace openmsx
