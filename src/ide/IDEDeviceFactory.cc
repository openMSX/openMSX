// $Id$

#include "IDEDeviceFactory.hh"
#include "DummyIDEDevice.hh"
#include "IDEHD.hh"
#include "IDECDROM.hh"
#include "DeviceConfig.hh"
#include "MSXException.hh"
#include "serialize.hh"

using std::auto_ptr;

namespace openmsx {
namespace IDEDeviceFactory {

auto_ptr<IDEDevice> create(MSXMotherBoard& motherBoard, const DeviceConfig& config)
{
	if (!config.getXML()) {
		return auto_ptr<IDEDevice>(new DummyIDEDevice());
	}
	const std::string& type = config.getChildData("type");
	if (type == "IDEHD") {
		return auto_ptr<IDEDevice>(new IDEHD(motherBoard, config));
	} else if (type == "IDECDROM") {
		return auto_ptr<IDEDevice>(new IDECDROM(motherBoard, config));
	}
	throw MSXException("Unknown IDE device: " + type);
}

} // namespace IDEDeviceFactory
} // namespace openmsx
