#include "ProgrammableDevice.hh"

#include "CommandController.hh"
#include "MSXCliComm.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "StringSetting.hh"
#include "serialize.hh"

#include "narrow.hh"

namespace openmsx {

static IterableBitSet<256> parseAllPorts(const Setting& setting);

ProgrammableDevice::ProgrammableDevice(const DeviceConfig& config)
	: MSXDevice(config)
	, portListSetting(
		getCommandController(), tmpStrCat(getName(), " ports"),
		"List of ports that are added to the pool of I/O ports this extension scans for", "")
	, resetCallback(getCommandController(),
		getName() + " reset callback",
		"Tcl proc to call when reset was detected, no parameters and return values are "
		"used", "", Setting::Save::NO)
	, inCallback(getCommandController(),
		tmpStrCat(getName(), " input callback"),
		"Defines a Tcl proc that receives a port number (byte) and returns a byte when "
		"the MSX is reading from a port of the device", "", Setting::Save::NO)
	, outCallback(getCommandController(),
		tmpStrCat(getName(), " output callback"),
		"Defines a Tcl proc that receives a port number (byte) and a byte when the "
		"MSX is writing to a port from the device", "", Setting::Save::NO)
{
	portListSetting.attach(*this);
	try {
		outPorts = inPorts = parseAllPorts(portListSetting);
	} catch (MSXException&) {
		// silently ignore error in settings.xml
		portListSetting.setString("");
	}
}

ProgrammableDevice::~ProgrammableDevice()
{
	portListSetting.detach(*this);
}

static IterableBitSet<256> parseAllPorts(const Setting& setting)
{
	Interpreter& interp = setting.getInterpreter();
	TclObject portList = setting.getValue();
	IterableBitSet<256> ports;

	for (unsigned n = 0; n < portList.getListLength(interp); n++) {
		int tmp = portList.getListIndex(interp, n).getInt(interp);
		if ((tmp < 0) || (tmp > 255)) {
		        throw MSXException("outside range 0..255");
		}
		ports.set(narrow_cast<byte>(tmp));
	}
	return ports;
}

void ProgrammableDevice::update(const Setting& setting) noexcept
{
	try {
		IterableBitSet<256> tmpPorts = parseAllPorts(setting);
		unregisterPorts();
		inPorts = tmpPorts;
		outPorts = tmpPorts;
		doRegisterPorts();
	} catch (MSXException& e) {
		getCliComm().printWarning(
			"Parse error in \"",
			portListSetting.getFullName(), "\": ", e.getMessage());
	}
}

void ProgrammableDevice::writeIO(word port, byte value, EmuTime::param /*time*/)
{
	outCallback.execute(port, value);
}

byte ProgrammableDevice::readIO(word port, EmuTime::param /* time */)
{
	try {
		auto obj = inCallback.execute(port);
		int tmp = obj.getInt(getCommandController().getInterpreter());
		if ((tmp < 0) || (tmp > 255)) {
		        throw MSXException("outside range 0..255");
		}
		return narrow_cast<byte>(tmp);
	} catch (MSXException& e) {
		getCliComm().printWarning(
			"Wrong result for callback function \"",
			inCallback.getSetting().getFullName(),
			"\": ", e.getMessage());
		return 255;
	}
}

void ProgrammableDevice::reset(EmuTime::param /*time*/)
{
	resetCallback.execute();
}

REGISTER_MSXDEVICE(ProgrammableDevice, "ProgrammableDevice");

} // namespace openmsx
