#include "MSXMemoryMapper.hh"

#include "serialize.hh"

namespace openmsx {

MSXMemoryMapper::MSXMemoryMapper(const DeviceConfig& config)
	: MSXMemoryMapperBase(config)
{
}

void MSXMemoryMapper::writeIO(uint16_t port, byte value, EmuTime time)
{
	MSXMemoryMapperBase::writeIOImpl(port, value, time);
	byte page = port & 3;
	if (byte* data = checkedRam.getRWCacheLines(segmentOffset(page), 0x4000)) {
		fillDeviceRWCache(page * 0x4000, 0x4000, data);
	} else {
		invalidateDeviceRWCache(page * 0x4000, 0x4000);
	}
}

template<typename Archive>
void MSXMemoryMapper::serialize(Archive& ar, unsigned version)
{
	// use serializeInlinedBase instead of serializeBase for bw-compat savestates
	ar.template serializeInlinedBase<MSXMemoryMapperBase>(*this, version);
}

INSTANTIATE_SERIALIZE_METHODS(MSXMemoryMapper);
REGISTER_MSXDEVICE(MSXMemoryMapper, "MemoryMapper");

} // namespace openmsx
