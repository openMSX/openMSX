// $Id$

#include "VDPIODelay.hh"
#include "MSXCPU.hh"
#include "Config.hh"
#include "FileContext.hh"

namespace openmsx {

Config* VDPIODelay::getConfig()
{
	XMLElement deviceElem("VDPIODelay");
	SystemFileContext dummyContext;
	static Config config(deviceElem, dummyContext);
	return &config;
}

VDPIODelay::VDPIODelay(MSXIODevice* device_, const EmuTime& time)
	: MSXDevice(getConfig(), time),
	  MSXIODevice(getConfig(), time),
	  cpu(MSXCPU::instance()),
	  device(device_)
{
}

byte VDPIODelay::readIO(byte port, const EmuTime& time)
{
	const EmuTime& time2 = delay(time);
	return device->readIO(port, time2);
}

void VDPIODelay::writeIO(byte port, byte value, const EmuTime& time)
{
	const EmuTime& time2 = delay(time);
	device->writeIO(port, value, time2);
}

const EmuTime& VDPIODelay::delay(const EmuTime& time)
{
	if (cpu.isR800Active()) {
		lastTime += 57;	// 8us
		if (time < lastTime) {
			cpu.wait(lastTime);
			return lastTime;
		}
	}
	lastTime = time;
	return lastTime;
}

} // namespace openmsx
