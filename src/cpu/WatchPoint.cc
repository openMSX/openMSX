#include "WatchPoint.hh"

#include "Interpreter.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "StateChangeDistributor.hh"
#include "TclObject.hh"

#include "checked_cast.hh"
#include "one_of.hh"

#include <algorithm>
#include <cassert>
#include <memory>

namespace openmsx {

// class WatchPoint

void WatchPoint::registerIOWatch(MSXMotherBoard& motherBoard, std::span<MSXDevice*, 256> devices)
{
	assert(!registered);
	assert(getType() == one_of(Type::READ_IO, Type::WRITE_IO));
	if (!beginAddr || !endAddr) return;
	assert(*beginAddr < 0x100 && *endAddr < 0x100 && *beginAddr <= *endAddr);
	for (unsigned port = *beginAddr; port <= *endAddr; ++port) {
		// create new MSXWatchIOdevice ...
		auto& dev = ios.emplace_back(std::make_unique<MSXWatchIODevice>(
			*motherBoard.getMachineConfig(), *this));
		// ... and insert (prepend) in chain
		dev->getDevicePtr() = devices[port];
		devices[port] = dev.get();
	}
	registered = true;
}

void WatchPoint::unregisterIOWatch(std::span<MSXDevice*, 256> devices)
{
	assert(registered);
	assert(getType() == one_of(Type::READ_IO, Type::WRITE_IO));
	if (!beginAddr || !endAddr) return;
	assert(*beginAddr < 0x100 && *endAddr < 0x100 && *beginAddr <= *endAddr);
	for (unsigned port = *beginAddr; port <= *endAddr; ++port) {
		// find pointer to watchpoint
		MSXDevice** prev = &devices[port];
		while (*prev != ios[port - *beginAddr].get()) {
			prev = &checked_cast<MSXWatchIODevice*>(*prev)->getDevicePtr();
		}
		// remove watchpoint from chain
		*prev = checked_cast<MSXWatchIODevice*>(*prev)->getDevicePtr();
	}
	ios.clear();
	registered = false;
}

void WatchPoint::doReadCallback(MSXMotherBoard& motherBoard, unsigned port)
{
	auto& cpuInterface = motherBoard.getCPUInterface();
	if (cpuInterface.isFastForward()) return;

	auto& reactor = motherBoard.getReactor();
	auto& cliComm = reactor.getGlobalCliComm();
	auto& interp  = reactor.getInterpreter();
	interp.setVariable(TclObject("wp_last_address"), TclObject(int(port)));

	// keep this object alive by holding a shared_ptr to it, for the case
	// this watchpoint deletes itself in checkAndExecute()
	auto keepAlive = shared_from_this();
	auto scopedBlock = motherBoard.getStateChangeDistributor().tempBlockNewEventsDuringReplay();
	if (bool remove = checkAndExecute(cliComm, interp); remove) {
		cpuInterface.removeWatchPoint(keepAlive);
	}

	interp.unsetVariable("wp_last_address");
}

void WatchPoint::doWriteCallback(MSXMotherBoard& motherBoard, unsigned port, unsigned value)
{
	auto& cpuInterface = motherBoard.getCPUInterface();
	if (cpuInterface.isFastForward()) return;

	auto& reactor = motherBoard.getReactor();
	auto& cliComm = reactor.getGlobalCliComm();
	auto& interp  = reactor.getInterpreter();
	interp.setVariable(TclObject("wp_last_address"), TclObject(int(port)));
	interp.setVariable(TclObject("wp_last_value"),   TclObject(int(value)));

	// see comment in doReadCallback() above
	auto keepAlive = shared_from_this();
	auto scopedBlock = motherBoard.getStateChangeDistributor().tempBlockNewEventsDuringReplay();
	if (bool remove = checkAndExecute(cliComm, interp); remove) {
		cpuInterface.removeWatchPoint(keepAlive);
	}

	interp.unsetVariable("wp_last_address");
	interp.unsetVariable("wp_last_value");
}


// class MSXWatchIODevice

MSXWatchIODevice::MSXWatchIODevice(
		const HardwareConfig& hwConf, WatchPoint& wp_)
	: MSXMultiDevice(hwConf)
	, wp(wp_)
{
}

const std::string& MSXWatchIODevice::getName() const
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
	wp.doReadCallback(getMotherBoard(), port);
	return device->readIO(port, time);
}

void MSXWatchIODevice::writeIO(word port, byte value, EmuTime::param time)
{
	assert(device);

	// first write to device, then trigger watchpoint
	device->writeIO(port, value, time);
	wp.doWriteCallback(getMotherBoard(), port, value);
}

} // namespace openmsx
