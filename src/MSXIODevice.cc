// $Id$

#include "MSXIODevice.hh"
#include "MSXCPUInterface.hh"
#include "xmlx.hh"
#include "StringOp.hh"

namespace openmsx {

static void getPorts(const XMLElement& config,
                     vector<byte>& inPorts, vector<byte>& outPorts)
{
	XMLElement::Children ios;
	config.getChildren("io", ios);
	for (XMLElement::Children::const_iterator it = ios.begin();
	     it != ios.end(); ++it) {
		unsigned base = StringOp::stringToInt((*it)->getAttribute("base"));
		unsigned num  = StringOp::stringToInt((*it)->getAttribute("num"));
		string type = (*it)->getAttribute("type", "IO");
		if (((base + num) > 256) || (num == 0) ||
		    ((type != "I") && (type != "O") && (type != "IO"))) {
			throw FatalError("Invalid IO port specification");
		}
		for (unsigned i = 0; i < num; ++i) {
			if ((type == "I") || (type == "IO")) {
				inPorts.push_back(base + i);
			}
			if ((type == "O") || (type == "IO")) {
				outPorts.push_back(base + i);
			}
		}
	}
}

MSXIODevice::MSXIODevice(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
{
	vector<byte> inPorts;
	vector<byte> outPorts;
	getPorts(config, inPorts, outPorts);
	
	for (vector<byte>::iterator it = inPorts.begin();
	     it != inPorts.end(); ++it) {
		MSXCPUInterface::instance().register_IO_In(*it, this);
	}
	for (vector<byte>::iterator it = outPorts.begin();
	     it != outPorts.end(); ++it) {
		MSXCPUInterface::instance().register_IO_Out(*it, this);
	}
}

MSXIODevice::~MSXIODevice()
{
	vector<byte> inPorts;
	vector<byte> outPorts;
	getPorts(deviceConfig, inPorts, outPorts);
	
	MSXCPUInterface& cpuInterface = MSXCPUInterface::instance();
	for (vector<byte>::iterator it = inPorts.begin();
	     it != inPorts.end(); ++it) {
		cpuInterface.unregister_IO_In(*it, this);
	}
	for (vector<byte>::iterator it = outPorts.begin();
	     it != outPorts.end(); ++it) {
		cpuInterface.unregister_IO_Out(*it, this);
	}
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
