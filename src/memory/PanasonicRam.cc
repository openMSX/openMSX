#include "PanasonicRam.hh"
#include "MSXMotherBoard.hh"
#include "PanasonicMemory.hh"
#include "serialize.hh"

namespace openmsx {

PanasonicRam::PanasonicRam(const DeviceConfig& config)
	: MSXMemoryMapper(config)
	, panasonicMemory(getMotherBoard().getPanasonicMemory())
{
	panasonicMemory.registerRam(checkedRam.getUncheckedRam());
}

void PanasonicRam::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	unsigned addr = calcAddress(address);
	if (panasonicMemory.isWritable(addr)) {
		checkedRam.write(addr, value);
	}
}

byte* PanasonicRam::getWriteCacheLine(word start) const
{
	unsigned addr = calcAddress(start);
	if (panasonicMemory.isWritable(addr)) {
		return checkedRam.getWriteCacheLine(addr);
	} else {
		return unmappedWrite;
	}
}

template<typename Archive>
void PanasonicRam::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXMemoryMapper>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(PanasonicRam);
REGISTER_MSXDEVICE(PanasonicRam, "PanasonicRam");

} // namespace openmsx
