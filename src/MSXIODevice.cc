// $Id$

#include "MSXIODevice.hh"
#include "MSXCPUInterface.hh"
#include "xmlx.hh"
#include "StringOp.hh"

namespace openmsx {

MSXIODevice::MSXIODevice(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
{
	XMLElement::Children ios;
	config.getChildren("io", ios);
	for (XMLElement::Children::const_iterator it = ios.begin();
	     it != ios.end(); ++it) {
		unsigned base = StringOp::stringToInt((*it)->getAttribute("base"));
		unsigned num  = StringOp::stringToInt((*it)->getAttribute("num"));
		string   type = (*it)->getAttribute("type");
		if (type.empty()) {
			type = "IO";
		}
		if (((base + num) > 256) || (num == 0) ||
		    ((type != "I") && (type != "O") && (type != "IO"))) {
			throw FatalError("Invalid IO port specification");
		}
		MSXCPUInterface& cpuInterface = MSXCPUInterface::instance();
		for (unsigned i = 0; i < num; ++i) {
			if ((type == "I") || (type == "IO")) {
				cpuInterface.register_IO_In(base + i, this);
			}
			if ((type == "O") || (type == "IO")) {
				cpuInterface.register_IO_Out(base + i, this);
			}
		}
	}
}

MSXIODevice::~MSXIODevice()
{
}

byte MSXIODevice::readIO(byte port, const EmuTime& /*time*/)
{
	PRT_DEBUG("MSXIODevice::readIO (0x" << hex << (int)port << dec
		<< ") : No device implementation.");
	return 255;
}
void MSXIODevice::writeIO(byte port, byte value, const EmuTime& /*time*/)
{
	PRT_DEBUG("MSXIODevice::writeIO(port 0x" << hex << (int)port << dec
		<<",value "<<(int)value<<") : No device implementation.");
	// do nothing
}

} // namespace openmsx
