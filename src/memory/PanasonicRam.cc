#include "PanasonicRam.hh"
#include "MSXMotherBoard.hh"
#include "PanasonicMemory.hh"
#include "serialize.hh"

namespace openmsx {

PanasonicRam::PanasonicRam(const DeviceConfig& config)
	: MSXMemoryMapperBase(config)
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

byte* PanasonicRam::getWriteCacheLine(word start)
{
	unsigned addr = calcAddress(start);
	if (panasonicMemory.isWritable(addr)) {
		return checkedRam.getWriteCacheLine(addr);
	} else {
		return unmappedWrite.data();
	}
}

void PanasonicRam::writeIO(word port, byte value, EmuTime::param time)
{
	MSXMemoryMapperBase::writeIOImpl(port, value, time);
	byte page = port & 3;
	unsigned addr = segmentOffset(page);
	if (byte* data = checkedRam.getRWCacheLines(addr, 0x4000)) {
		const byte* rData = data;
		byte* wData = panasonicMemory.isWritable(addr) ? data : unmappedWrite.data();
		fillDeviceRWCache(page * 0x4000, 0x4000, rData, wData);
	} else {
		invalidateDeviceRWCache(page * 0x4000, 0x4000);
	}
}

template<typename Archive>
void PanasonicRam::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXMemoryMapperBase>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(PanasonicRam);
REGISTER_MSXDEVICE(PanasonicRam, "PanasonicRam");

} // namespace openmsx
