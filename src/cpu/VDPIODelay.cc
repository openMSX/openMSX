// $Id$

#include "VDPIODelay.hh"
#include "MSXCPU.hh"


namespace openmsx {

VDPIODelay::VDPIODelay(MSXIODevice *device_, const EmuTime &time)
	: MSXDevice(device_->deviceConfig, time),
	  MSXIODevice(device_->deviceConfig, time),
	  device(device_)
{
	cpu = MSXCPU::instance();
}

byte VDPIODelay::readIO(byte port, const EmuTime &time)
{
	const EmuTime &time2 = delay(time);
	return device->readIO(port, time2);
}

void VDPIODelay::writeIO(byte port, byte value, const EmuTime &time)
{
	const EmuTime &time2 = delay(time);
	device->writeIO(port, value, time2);
}

const EmuTime &VDPIODelay::delay(const EmuTime &time)
{
	lastTime += 57;	// 8us
	if (time >= lastTime || !cpu->waitR800(lastTime)) {
		lastTime = time;
	}
	return lastTime;
}

} // namespace openmsx
