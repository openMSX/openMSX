#include "VDPIODelay.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "DummyDevice.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {

VDPIODelay::VDPIODelay(const DeviceConfig& config, MSXCPUInterface& cpuInterface)
	: MSXDevice(config)
	, cpu(getCPU()) // used frequently, so cache it
{
	for (auto port : xrange(uint8_t(0x98), uint8_t(0x9c))) {
		getInDevicePtr (port) = &cpuInterface.getDummyDevice();
		getOutDevicePtr(port) = &cpuInterface.getDummyDevice();
	}
}

const MSXDevice& VDPIODelay::getInDevice(uint8_t port) const
{
	assert((0x98 <= port) && (port <= 0x9B));
	return *inDevices[port - 0x98];
}

MSXDevice*& VDPIODelay::getInDevicePtr(uint8_t port)
{
	assert((0x98 <= port) && (port <= 0x9B));
	return inDevices[port - 0x98];
}

MSXDevice*& VDPIODelay::getOutDevicePtr(uint8_t port)
{
	assert((0x98 <= port) && (port <= 0x9B));
	return outDevices[port - 0x98];
}

uint8_t VDPIODelay::readIO(uint16_t port, EmuTime time)
{
	delay(time);
	return getInDevicePtr(uint8_t(port))->readIO(uint8_t(port), lastTime.getTime());
}

uint8_t VDPIODelay::peekIO(uint16_t port, EmuTime time) const
{
	return getInDevice(uint8_t(port)).peekIO(uint8_t(port), time);
}

void VDPIODelay::writeIO(uint16_t port, uint8_t value, EmuTime time)
{
	delay(time);
	getOutDevicePtr(uint8_t(port))->writeIO(uint8_t(port), value, lastTime.getTime());
}

void VDPIODelay::delay(EmuTime time)
{
	if (cpu.isR800Active()) {
		// Number of cycles based on measurements on real HW.
		// See doc/turbor-vdp-io-timing.ods for details.
		lastTime += 62; // 8us
		if (time < lastTime.getTime()) {
			cpu.wait(lastTime.getTime());
			return;
		}
	}
	lastTime.advance(time);
}

template<typename Archive>
void VDPIODelay::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("lastTime", lastTime);
}
INSTANTIATE_SERIALIZE_METHODS(VDPIODelay);

} // namespace openmsx
