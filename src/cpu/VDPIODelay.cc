// $Id$

#include "VDPIODelay.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "DummyDevice.hh"
#include <cassert>

namespace openmsx {

VDPIODelay::VDPIODelay(MSXMotherBoard& motherboard, const XMLElement& config,
                       const EmuTime& time)
	: MSXDevice(motherboard, config, time)
	, cpu(motherBoard.getCPU())
	, lastTime(time)
{
	for (int port = 0x098; port <= 0x9B; ++port) {
		getInDevicePtr (port) = &motherBoard.getDummyDevice();
		getOutDevicePtr(port) = &motherBoard.getDummyDevice();
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

byte VDPIODelay::readIO(word port, const EmuTime& time)
{
	delay(time);
	return getInDevicePtr(port)->readIO(port, lastTime.getTime());
}

byte VDPIODelay::peekIO(word port, const EmuTime& time) const
{
	return getInDevice(port).peekIO(port, time);
}

void VDPIODelay::writeIO(word port, byte value, const EmuTime& time)
{
	delay(time);
	getOutDevicePtr(port)->writeIO(port, value, lastTime.getTime());
}

void VDPIODelay::delay(const EmuTime& time)
{
	if (cpu.isR800Active()) {
		lastTime += 57;	// 8us
		if (time < lastTime.getTime()) {
			cpu.wait(lastTime.getTime());
			return;
		}
	}
	lastTime.advance(time);
}

} // namespace openmsx
