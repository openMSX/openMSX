#include "ProgrammableDevice.hh"

#include "CommandController.hh"
#include "MSXCliComm.hh"
#include "MSXException.hh"

#include "narrow.hh"
#include "xrange.hh"

namespace openmsx {

[[nodiscard]] static IterableBitSet<256> parseAllPorts(const Setting& setting)
{
	Interpreter& interp = setting.getInterpreter();
	const auto& portList = setting.getValue();
	IterableBitSet<256> ports;

	for (auto n : xrange(portList.getListLength(interp))) {
		int tmp = portList.getListIndex(interp, n).getInt(interp);
		if ((tmp < 0) || (tmp > 255)) {
		        throw MSXException("outside range 0..255");
		}
		ports.set(narrow_cast<byte>(tmp));
	}
	return ports;
}

ProgrammableDevice::ProgrammableDevice(const DeviceConfig& config)
	: MSXDevice(config)
	, portListSetting(getCommandController(),
		tmpStrCat(getName(), " ports"),
		"List of ports that are added to the pool of I/O ports this extension scans for", "")
	, resetCallback(getCommandController(),
		tmpStrCat(getName(), " reset callback"),
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
		auto ports = parseAllPorts(portListSetting);
		inPorts  = ports;
		outPorts = ports;
	} catch (MSXException&) {
		// silently ignore error in settings.xml
		portListSetting.setString("");
	}
}

ProgrammableDevice::~ProgrammableDevice()
{
	portListSetting.detach(*this);
}

void ProgrammableDevice::update(const Setting& setting) noexcept
{
	try {
		auto ports = parseAllPorts(setting);
		unregisterPorts();
		inPorts  = ports;
		outPorts = ports;
		doRegisterPorts();
	} catch (MSXException& e) {
		getCliComm().printWarning(
			"Parse error in \"",
			portListSetting.getFullName(), "\": ", e.getMessage());
	}
}

void ProgrammableDevice::writeIO(uint16_t port, byte value, EmuTime::param /*time*/)
{
	outCallback.execute(port & 0xff, value);
}

byte ProgrammableDevice::readIO(uint16_t port, EmuTime::param /*time*/)
{
	try {
		auto obj = inCallback.execute(port & 0xff);
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
		return 0xff;
	}
}

void ProgrammableDevice::reset(EmuTime::param /*time*/)
{
	resetCallback.execute();
}

REGISTER_MSXDEVICE(ProgrammableDevice, "ProgrammableDevice");

} // namespace openmsx
