// $Id$

#include "MSXWatchIODevice.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "MSXCPUInterface.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include <tcl.h>
#include <cassert>

namespace openmsx {

// class WatchIO

WatchIO::WatchIO(MSXMotherBoard& motherboard,
                 WatchPoint::Type type,
                 unsigned beginAddr, unsigned endAddr,
                 std::auto_ptr<TclObject> command,
                 std::auto_ptr<TclObject> condition)
	: WatchPoint(motherboard.getReactor().getGlobalCliComm(), command,
	             condition, type, beginAddr, endAddr)
	, cpuInterface(motherboard.getCPUInterface())
{
	for (unsigned i = byte(beginAddr); i <= byte(endAddr); ++i) {
		ios.push_back(new MSXWatchIODevice(motherboard, *this));
	}
}

WatchIO::~WatchIO()
{
	for (IOs::iterator it = ios.begin(); it != ios.end(); ++it) {
		delete *it;
	}
}

MSXWatchIODevice& WatchIO::getDevice(byte port)
{
	byte begin = getBeginAddress();
	return *ios[port - begin];
}

void WatchIO::doReadCallback(unsigned port)
{
	Tcl_Interp* interp = getInterpreter();
	Tcl_SetVar(interp, "wp_last_address",
	           StringOp::toString(port).c_str(), TCL_GLOBAL_ONLY);

	// keep this object alive by holding a shared_ptr to it, for the case
	// this watchpoint deletes itself in checkAndExecute()
	// TODO can be implemented more efficiently by using
	//    std::shared_ptr::shared_from_this
	MSXCPUInterface::WatchPoints wpCopy(cpuInterface.getWatchPoints());
	checkAndExecute();

	Tcl_UnsetVar(interp, "wp_last_address", TCL_GLOBAL_ONLY);
}

void WatchIO::doWriteCallback(unsigned port, unsigned value)
{
	Tcl_Interp* interp = getInterpreter();
	Tcl_SetVar(interp, "wp_last_address",
	           StringOp::toString(port).c_str(), TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "wp_last_value",
	           StringOp::toString(value).c_str(), TCL_GLOBAL_ONLY);

	// see comment in doReadCallback() above
	MSXCPUInterface::WatchPoints wpCopy(cpuInterface.getWatchPoints());
	checkAndExecute();

	Tcl_UnsetVar(interp, "wp_last_address", TCL_GLOBAL_ONLY);
	Tcl_UnsetVar(interp, "wp_last_value",   TCL_GLOBAL_ONLY);
}


// class MSXWatchIODevice

MSXWatchIODevice::MSXWatchIODevice(
		MSXMotherBoard& motherboard, WatchIO& watchIO_)
	: MSXMultiDevice(motherboard)
	, watchIO(watchIO_)
	, device(0)
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
