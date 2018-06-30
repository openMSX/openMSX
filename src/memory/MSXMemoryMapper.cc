#include "MSXMemoryMapper.hh"
#include "MSXMotherBoard.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "memory.hh"
#include "outer.hh"
#include "Ram.hh" // because we serialize Ram instead of CheckedRam
#include "Math.hh"

namespace openmsx {

unsigned MSXMemoryMapper::getRamSize() const
{
	int kSize = getDeviceConfig().getChildDataAsInt("size");
	if ((kSize % 16) != 0) {
		throw MSXException(StringOp::Builder() <<
			"Mapper size is not a multiple of 16K: " << kSize);
	}
	if (kSize == 0) {
		throw MSXException("Mapper size must be at least 16kB.");
	}
	return kSize * 1024; // in bytes
}

MSXMemoryMapper::MSXMemoryMapper(const DeviceConfig& config)
	: MSXDevice(config)
	, checkedRam(config, getName(), "memory mapper", getRamSize())
	, debuggable(getMotherBoard(), getName())
{
}

MSXMemoryMapper::~MSXMemoryMapper() = default;

void MSXMemoryMapper::powerUp(EmuTime::param time)
{
	checkedRam.clear();
	reset(time);
}

void MSXMemoryMapper::reset(EmuTime::param /*time*/)
{
	// Most mappers initialize to segment 0 for all pages.
	// On MSX2 and higher, the BIOS will select segments 3..0 for pages 0..3.
	for (auto& reg : registers) {
		reg = 0;
	}
}

byte MSXMemoryMapper::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MSXMemoryMapper::peekIO(word port, EmuTime::param /*time*/) const
{
	unsigned numSegments = checkedRam.getSize() / 0x4000;
	return registers[port & 0x03] | ~(Math::powerOfTwo(numSegments) - 1);
}

void MSXMemoryMapper::writeIO(word port, byte value, EmuTime::param /*time*/)
{
	unsigned numSegments = checkedRam.getSize() / 0x4000;
	registers[port & 0x03] = value & (Math::powerOfTwo(numSegments) - 1);
}

unsigned MSXMemoryMapper::calcAddress(word address) const
{
	unsigned segment = registers[address >> 14];
	unsigned numSegments = checkedRam.getSize() / 0x4000;
	segment = (segment < numSegments) ? segment : segment & (numSegments - 1);
	return (segment << 14) | (address & 0x3FFF);
}

byte MSXMemoryMapper::peekMem(word address, EmuTime::param /*time*/) const
{
	return checkedRam.peek(calcAddress(address));
}

byte MSXMemoryMapper::readMem(word address, EmuTime::param /*time*/)
{
	return checkedRam.read(calcAddress(address));
}

void MSXMemoryMapper::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	checkedRam.write(calcAddress(address), value);
}

const byte* MSXMemoryMapper::getReadCacheLine(word start) const
{
	return checkedRam.getReadCacheLine(calcAddress(start));
}

byte* MSXMemoryMapper::getWriteCacheLine(word start) const
{
	return checkedRam.getWriteCacheLine(calcAddress(start));
}


// SimpleDebuggable

MSXMemoryMapper::Debuggable::Debuggable(MSXMotherBoard& motherBoard_,
                                        const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " regs",
	                   "Memory mapper registers", 4)
{
}

byte MSXMemoryMapper::Debuggable::read(unsigned address)
{
	auto& mapper = OUTER(MSXMemoryMapper, debuggable);
	return mapper.registers[address];
}

void MSXMemoryMapper::Debuggable::write(unsigned address, byte value)
{
	auto& mapper = OUTER(MSXMemoryMapper, debuggable);
	mapper.writeIO(address, value, EmuTime::dummy());
}


template<typename Archive>
void MSXMemoryMapper::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("registers", registers);
	}
	// TODO ar.serialize("checkedRam", checkedRam);
	ar.serialize("ram", checkedRam.getUncheckedRam());
}
INSTANTIATE_SERIALIZE_METHODS(MSXMemoryMapper);
REGISTER_MSXDEVICE(MSXMemoryMapper, "MemoryMapper");

} // namespace openmsx
