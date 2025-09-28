#include "MSXMirrorDevice.hh"

#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "serialize.hh"

namespace openmsx {

[[nodiscard]] static unsigned getAddressHigh(const DeviceConfig& config)
{
	unsigned prim = config.getChildDataAsInt("ps", 0);
	unsigned sec  = config.getChildDataAsInt("ss", 0);
	if ((prim >= 4) || (sec >= 4)) {
		throw MSXException("Invalid slot in mirror device.");
	}
	return (prim << 18) | (sec << 16);
}

MSXMirrorDevice::MSXMirrorDevice(const DeviceConfig& config)
	: MSXDevice(config)
	, interface(getCPUInterface()) // frequently used, so cache
	, addressHigh(getAddressHigh(config))
{
}

byte MSXMirrorDevice::peekMem(uint16_t address, EmuTime time) const
{
	return interface.peekSlottedMem(addressHigh | address, time);
}

byte MSXMirrorDevice::readMem(uint16_t address, EmuTime time)
{
	return interface.readSlottedMem(addressHigh | address, time);
}

void MSXMirrorDevice::writeMem(uint16_t address, byte value, EmuTime time)
{
	interface.writeSlottedMem(addressHigh | address, value, time);
}

const byte* MSXMirrorDevice::getReadCacheLine(uint16_t /*start*/) const
{
	return nullptr;
}

byte* MSXMirrorDevice::getWriteCacheLine(uint16_t /*start*/)
{
	return nullptr;
}

bool MSXMirrorDevice::allowUnaligned() const
{
	// OK, because this device doesn't call any 'fillDeviceXXXCache()'functions.
	return true;
}

template<typename Archive>
void MSXMirrorDevice::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMirrorDevice);
REGISTER_MSXDEVICE(MSXMirrorDevice, "Mirror");

} // namespace openmsx
