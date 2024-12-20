#include "MSXWatchIODevice.hh"

#include "Interpreter.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "StateChangeDistributor.hh"
#include "TclObject.hh"

#include "checked_cast.hh"
#include "one_of.hh"

#include <cassert>
#include <memory>

namespace openmsx {

// class WatchIO

WatchIO::WatchIO(WatchPoint::Type type_,
                 unsigned beginAddr_, unsigned endAddr_,
                 TclObject command_, TclObject condition_,
                 bool once_, unsigned newId /*= -1*/)
	: WatchPoint(std::move(command_), std::move(condition_), type_, beginAddr_, endAddr_, once_, newId)
{
}

void WatchIO::registerIOWatch(MSXMotherBoard& motherBoard, std::span<MSXDevice*, 256> devices)
{
	assert(getType() == one_of(Type::READ_IO, Type::WRITE_IO));
	unsigned beginPort = getBeginAddress();
	unsigned endPort   = getEndAddress();
	assert(beginPort <= endPort);
	assert(endPort < 0x100);
	for (unsigned port = beginPort; port <= endPort; ++port) {
		// create new MSXWatchIOdevice ...
		auto& dev = ios.emplace_back(std::make_unique<MSXWatchIODevice>(
			*motherBoard.getMachineConfig(), *this));
		// ... and insert (prepend) in chain
		dev->getDevicePtr() = devices[port];
		devices[port] = dev.get();
	}
}

void WatchIO::unregisterIOWatch(std::span<MSXDevice*, 256> devices)
{
	assert(getType() == one_of(Type::READ_IO, Type::WRITE_IO));
	unsigned beginPort = getBeginAddress();
	unsigned endPort   = getEndAddress();
	assert(beginPort <= endPort);
	assert(endPort < 0x100);

	for (unsigned port = beginPort; port <= endPort; ++port) {
		// find pointer to watchpoint
		MSXDevice** prev = &devices[port];
		while (*prev != ios[port - beginPort].get()) {
			prev = &checked_cast<MSXWatchIODevice*>(*prev)->getDevicePtr();
		}
		// remove watchpoint from chain
		*prev = checked_cast<MSXWatchIODevice*>(*prev)->getDevicePtr();
	}
	ios.clear();
}

void WatchIO::doReadCallback(MSXMotherBoard& motherBoard, unsigned port)
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

void WatchIO::doWriteCallback(MSXMotherBoard& motherBoard, unsigned port, unsigned value)
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
		const HardwareConfig& hwConf, WatchIO& watchIO_)
	: MSXMultiDevice(hwConf)
	, watchIO(watchIO_)
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
	watchIO.doReadCallback(getMotherBoard(), port);
	return device->readIO(port, time);
}

void MSXWatchIODevice::writeIO(word port, byte value, EmuTime::param time)
{
	assert(device);

	// first write to device, then trigger watchpoint
	device->writeIO(port, value, time);
	watchIO.doWriteCallback(getMotherBoard(), port, value);
}

} // namespace openmsx
