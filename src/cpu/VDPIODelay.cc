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
	, lastTime(EmuTime::zero())
{
	for (auto port : xrange(byte(0x98), byte(0x9c))) {
		getInDevicePtr (port) = &cpuInterface.getDummyDevice();
		getOutDevicePtr(port) = &cpuInterface.getDummyDevice();
	}
}

const MSXDevice& VDPIODelay::getInDevice(byte port) const
{
	assert((0x98 <= port) && (port <= 0x9B));
	return *inDevices[port - 0x98];
}

MSXDevice*& VDPIODelay::getInDevicePtr(byte port)
{
	assert((0x98 <= port) && (port <= 0x9B));
	return inDevices[port - 0x98];
}

MSXDevice*& VDPIODelay::getOutDevicePtr(byte port)
{
	assert((0x98 <= port) && (port <= 0x9B));
	return outDevices[port - 0x98];
}

byte VDPIODelay::readIO(word port, EmuTime::param time)
{
	delay(time);
	return getInDevicePtr(byte(port))->readIO(byte(port), lastTime.getTime());
}

byte VDPIODelay::peekIO(word port, EmuTime::param time) const
{
	return getInDevice(byte(port)).peekIO(byte(port), time);
}

void VDPIODelay::writeIO(word port, byte value, EmuTime::param time)
{
	delay(time);
	getOutDevicePtr(byte(port))->writeIO(byte(port), value, lastTime.getTime());
}

void VDPIODelay::delay(EmuTime::param time)
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
