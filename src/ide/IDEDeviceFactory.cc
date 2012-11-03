// $Id$

#include "IDEDeviceFactory.hh"
#include "DummyIDEDevice.hh"
#include "IDEHD.hh"
#include "IDECDROM.hh"
#include "DeviceConfig.hh"
#include "MSXException.hh"

using std::unique_ptr;

namespace openmsx {
namespace IDEDeviceFactory {

unique_ptr<IDEDevice> create(const DeviceConfig& config)
{
	if (!config.getXML()) {
		return unique_ptr<IDEDevice>(new DummyIDEDevice());
	}
	const std::string& type = config.getChildData("type");
	if (type == "IDEHD") {
		return unique_ptr<IDEDevice>(new IDEHD(config));
	} else if (type == "IDECDROM") {
		return unique_ptr<IDEDevice>(new IDECDROM(config));
	}
	throw MSXException("Unknown IDE device: " + type);
}

} // namespace IDEDeviceFactory
} // namespace openmsx
