// $Id$

#include "MSXWatchIODevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPUInterface.hh"
#include "TclObject.hh"
#include <cassert>

namespace openmsx {

MSXWatchIODevice::MSXWatchIODevice(MSXMotherBoard& motherboard,
                                   WatchPoint::Type type,
                                   unsigned beginAddr, unsigned endAddr,
                                   std::auto_ptr<TclObject> command,
                                   std::auto_ptr<TclObject> condition)
	: MSXMultiDevice(motherboard)
	, WatchPoint(motherboard.getMSXCliComm(), command, condition, type,
	             beginAddr, endAddr)
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

byte MSXWatchIODevice::peekIO(word port, EmuTime::param time) const
{
	assert(device);
	return device->peekIO(port, time);
}

byte MSXWatchIODevice::readIO(word port, EmuTime::param time)
{
	assert(device);
	//std::cout << "Watch readIO " << port << std::endl;
	byte result = device->readIO(port, time);

	// keep this object alive by holding a shared_ptr to it, for the case
	// this watchpoint deletes itself in checkAndExecute()
	// TODO can be implemented more efficiently by using
	//    std::shared_ptr::shared_from_this
	MSXCPUInterface::WatchPoints wpCopy(
		getMotherBoard().getCPUInterface().getWatchPoints());
	checkAndExecute();
	return result;
}

void MSXWatchIODevice::writeIO(word port, byte value, EmuTime::param time)
{
	assert(device);
	//std::cout << "Watch writeIO " << port << " " << value << std::endl;
	device->writeIO(port, value, time);

	// see comment in readIO() above
	MSXCPUInterface::WatchPoints wpCopy(
		getMotherBoard().getCPUInterface().getWatchPoints());
	checkAndExecute();
}

} // namespace openmsx
