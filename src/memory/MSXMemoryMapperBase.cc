#include "MSXMemoryMapperBase.hh"
#include "MSXException.hh"
#include "outer.hh"
#include "ranges.hh"
#include "serialize.hh"
#include <bit>

namespace openmsx {

[[nodiscard]] static unsigned getRamSize(const DeviceConfig& config)
{
	int kSize = config.getChildDataAsInt("size", 0);
	if ((kSize % 16) != 0) {
		throw MSXException("Mapper size is not a multiple of 16K: ", kSize);
	}
	if (kSize == 0) {
		throw MSXException("Mapper size must be at least 16kB.");
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

void MSXMemoryMapperBase::powerUp(EmuTime::param time)
{
	checkedRam.clear();
	reset(time);
}

void MSXMemoryMapperBase::reset(EmuTime::param /*time*/)
{
	// Most mappers initialize to segment 0 for all pages.
	// On MSX2 and higher, the BIOS will select segments 3..0 for pages 0..3.
	ranges::fill(registers, 0);
}

byte MSXMemoryMapperBase::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MSXMemoryMapperBase::peekIO(word port, EmuTime::param /*time*/) const
{
	unsigned numSegments = checkedRam.getSize() / 0x4000;
	return registers[port & 0x03] | ~(std::bit_ceil(numSegments) - 1);
}

void MSXMemoryMapperBase::writeIOImpl(word port, byte value, EmuTime::param /*time*/)
{
	unsigned numSegments = checkedRam.getSize() / 0x4000;
	registers[port & 3] = value & (std::bit_ceil(numSegments) - 1);
	// Note: subclasses are responsible for handling CPU cacheline stuff
}

unsigned MSXMemoryMapperBase::segmentOffset(byte page) const
{
	unsigned segment = registers[page];
	unsigned numSegments = checkedRam.getSize() / 0x4000;
	segment = (segment < numSegments) ? segment : segment & (numSegments - 1);
	return segment * 0x4000;
}

unsigned MSXMemoryMapperBase::calcAddress(word address) const
{
	return segmentOffset(address / 0x4000) | (address & 0x3fff);
}

byte MSXMemoryMapperBase::peekMem(word address, EmuTime::param /*time*/) const
{
	return checkedRam.peek(calcAddress(address));
}

byte MSXMemoryMapperBase::readMem(word address, EmuTime::param /*time*/)
{
	return checkedRam.read(calcAddress(address));
}

void MSXMemoryMapperBase::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	checkedRam.write(calcAddress(address), value);
}

const byte* MSXMemoryMapperBase::getReadCacheLine(word start) const
{
	return checkedRam.getReadCacheLine(calcAddress(start));
}

byte* MSXMemoryMapperBase::getWriteCacheLine(word start) const
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

void MSXMemoryMapperBase::Debuggable::write(unsigned address, byte value)
{
	auto& mapper = OUTER(MSXMemoryMapperBase, debuggable);
	mapper.writeIO(address, value, EmuTime::dummy());
}


void MSXMemoryMapperBase::serialize(Archive auto& ar, unsigned version)
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
