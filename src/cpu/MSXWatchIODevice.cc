#include "MSXWatchIODevice.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "MSXCPUInterface.hh"
#include "TclObject.hh"
#include "Interpreter.hh"
#include <cassert>

namespace openmsx {

// class WatchIO

WatchIO::WatchIO(MSXMotherBoard& motherboard_,
                 WatchPoint::Type type_,
                 unsigned beginAddr_, unsigned endAddr_,
                 TclObject command_, TclObject condition_,
                 unsigned newId /*= -1*/)
	: WatchPoint(command_, condition_, type_, beginAddr_, endAddr_, newId)
	, motherboard(motherboard_)
{
	for (unsigned i = byte(beginAddr_); i <= byte(endAddr_); ++i) {
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
	auto& cpuInterface = motherboard.getCPUInterface();
	if (cpuInterface.isFastForward()) return;

	auto& cliComm = motherboard.getReactor().getGlobalCliComm();
	auto& interp  = motherboard.getReactor().getInterpreter();
	interp.setVariable(TclObject("wp_last_address"), TclObject(int(port)));

	// keep this object alive by holding a shared_ptr to it, for the case
	// this watchpoint deletes itself in checkAndExecute()
	auto keepAlive = shared_from_this();
	checkAndExecute(cliComm, interp);

	interp.unsetVariable("wp_last_address");
}

void WatchIO::doWriteCallback(unsigned port, unsigned value)
{
	auto& cpuInterface = motherboard.getCPUInterface();
	if (cpuInterface.isFastForward()) return;

	auto& cliComm = motherboard.getReactor().getGlobalCliComm();
	auto& interp  = motherboard.getReactor().getInterpreter();
	interp.setVariable(TclObject("wp_last_address"), TclObject(int(port)));
	interp.setVariable(TclObject("wp_last_value"),   TclObject(int(value)));

	// see comment in doReadCallback() above
	auto keepAlive = shared_from_this();
	checkAndExecute(cliComm, interp);

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
