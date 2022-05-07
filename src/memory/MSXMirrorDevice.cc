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

byte MSXMirrorDevice::peekMem(word address, EmuTime::param time) const
{
	return interface.peekSlottedMem(addressHigh | address, time);
}

byte MSXMirrorDevice::readMem(word address, EmuTime::param time)
{
	return interface.readSlottedMem(addressHigh | address, time);
}

void MSXMirrorDevice::writeMem(word address, byte value, EmuTime::param time)
{
	interface.writeSlottedMem(addressHigh | address, value, time);
}

const byte* MSXMirrorDevice::getReadCacheLine(word /*start*/) const
{
	return nullptr;
}

byte* MSXMirrorDevice::getWriteCacheLine(word /*start*/) const
{
	return nullptr;
}

bool MSXMirrorDevice::allowUnaligned() const
{
	// OK, because this device doesn't call any 'fillDeviceXXXCache()'functions.
	return true;
}

void MSXMirrorDevice::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMirrorDevice);
REGISTER_MSXDEVICE(MSXMirrorDevice, "Mirror");

} // namespace openmsx
