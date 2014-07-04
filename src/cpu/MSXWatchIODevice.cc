#include "MSXWatchIODevice.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "MSXCPUInterface.hh"
#include "TclObject.hh"
#include "Interpreter.hh"
#include "StringOp.hh"
#include <cassert>

namespace openmsx {

// class WatchIO

WatchIO::WatchIO(MSXMotherBoard& motherboard,
                 WatchPoint::Type type,
                 unsigned beginAddr, unsigned endAddr,
                 TclObject command, TclObject condition,
                 unsigned newId /*= -1*/)
	: WatchPoint(motherboard.getReactor().getGlobalCliComm(),
	             motherboard.getReactor().getInterpreter(),
	             command, condition, type, beginAddr, endAddr, newId)
	, cpuInterface(motherboard.getCPUInterface())
{
	for (unsigned i = byte(beginAddr); i <= byte(endAddr); ++i) {
		ios.push_back(make_unique<MSXWatchIODevice>(
			*motherboard.getMachineConfig(), *this));
	}
}

WatchIO::~WatchIO()
{
}

MSXWatchIODevice& WatchIO::getDevice(byte port)
{
	byte begin = getBeginAddress();
	return *ios[port - begin];
}

void WatchIO::doReadCallback(unsigned port)
{
	if (cpuInterface.isFastForward()) return;

	auto& interp = getInterpreter();
	interp.setVariable("wp_last_address", StringOp::toString(port));

	// keep this object alive by holding a shared_ptr to it, for the case
	// this watchpoint deletes itself in checkAndExecute()
	// TODO can be implemented more efficiently by using
	//    std::shared_ptr::shared_from_this
	MSXCPUInterface::WatchPoints wpCopy(cpuInterface.getWatchPoints());
	checkAndExecute();

	interp.unsetVariable("wp_last_address");
}

void WatchIO::doWriteCallback(unsigned port, unsigned value)
{
	if (cpuInterface.isFastForward()) return;

	auto& interp = getInterpreter();
	interp.setVariable("wp_last_address", StringOp::toString(port));
	interp.setVariable("wp_last_value",   StringOp::toString(value));

	// see comment in doReadCallback() above
	MSXCPUInterface::WatchPoints wpCopy(cpuInterface.getWatchPoints());
	checkAndExecute();

	interp.unsetVariable("wp_last_address");
	interp.unsetVariable("wp_last_value");
}


// class MSXWatchIODevice

MSXWatchIODevice::MSXWatchIODevice(
		const HardwareConfig& hwConf, WatchIO& watchIO_)
	: MSXMultiDevice(hwConf)
	, watchIO(watchIO_)
	, device(nullptr)
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

	// first trigger watchpoint, then read from device
	watchIO.doReadCallback(port);
	return device->readIO(port, time);
}

void MSXWatchIODevice::writeIO(word port, byte value, EmuTime::param time)
{
	assert(device);

	// first write to device, then trigger watchpoint
	device->writeIO(port, value, time);
	watchIO.doWriteCallback(port, value);
}

} // namespace openmsx
