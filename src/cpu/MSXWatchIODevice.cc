// $Id$

#include "MSXWatchIODevice.hh"
#include "MSXMotherBoard.hh"
#include "TclObject.hh"
#include <cassert>

#include <iostream>

namespace openmsx {

MSXWatchIODevice::MSXWatchIODevice(MSXMotherBoard& motherboard,
                                   WatchPoint::Type type, unsigned address,
                                   std::auto_ptr<TclObject> command,
                                   std::auto_ptr<TclObject> condition)
	: MSXMultiDevice(motherboard)
	, WatchPoint(motherboard.getCliComm(), command, condition, type, address)
	, device(0)
{
}

MSXWatchIODevice::~MSXWatchIODevice()
{
}

MSXDevice*& MSXWatchIODevice::getDevicePtr()
{
	return device;
}

std::string MSXWatchIODevice::getName() const
{
	assert(device);
	return device->getName();
}

byte MSXWatchIODevice::peekIO(word port, const EmuTime& time) const
{
	assert(device);
	return device->peekIO(port, time);
}

byte MSXWatchIODevice::readIO(word port, const EmuTime& time)
{
	assert(device);
	std::cout << "Watch readIO " << port << std::endl;
	byte result = device->readIO(port, time);
	checkAndExecute();
	return result;
}

void MSXWatchIODevice::writeIO(word port, byte value, const EmuTime& time)
{
	assert(device);
	std::cout << "Watch writeIO " << port << " " << value << std::endl;
	device->writeIO(port, value, time);
	checkAndExecute();
}

} // namespace openmsx
