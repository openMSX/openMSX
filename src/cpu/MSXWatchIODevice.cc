// $Id$

#include "MSXWatchIODevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPUInterface.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include <tcl.h>
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

	Tcl_Interp* interp = getInterpreter();
	Tcl_SetVar(interp, "wp_last_address",
	           StringOp::toString(unsigned(port)).c_str(), TCL_GLOBAL_ONLY);

	// keep this object alive by holding a shared_ptr to it, for the case
	// this watchpoint deletes itself in checkAndExecute()
	// TODO can be implemented more efficiently by using
	//    std::shared_ptr::shared_from_this
	MSXCPUInterface::WatchPoints wpCopy(
		getMotherBoard().getCPUInterface().getWatchPoints());
	checkAndExecute();

	Tcl_UnsetVar(interp, "wp_last_address", TCL_GLOBAL_ONLY);

	// first trigger watchpoint, then read from device
	return device->readIO(port, time);
}

void MSXWatchIODevice::writeIO(word port, byte value, EmuTime::param time)
{
	assert(device);

	// first write to device, then trigger watchpoint
	device->writeIO(port, value, time);

	Tcl_Interp* interp = getInterpreter();
	Tcl_SetVar(interp, "wp_last_address",
	           StringOp::toString(unsigned(port)).c_str(), TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "wp_last_value",
	           StringOp::toString(unsigned(value)).c_str(), TCL_GLOBAL_ONLY);

	// see comment in readIO() above
	MSXCPUInterface::WatchPoints wpCopy(
		getMotherBoard().getCPUInterface().getWatchPoints());
	checkAndExecute();

	Tcl_UnsetVar(interp, "wp_last_address", TCL_GLOBAL_ONLY);
	Tcl_UnsetVar(interp, "wp_last_value",   TCL_GLOBAL_ONLY);
}

} // namespace openmsx
