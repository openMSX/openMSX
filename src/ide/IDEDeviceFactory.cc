#include "IDEDeviceFactory.hh"
#include "DummyIDEDevice.hh"
#include "IDEHD.hh"
#include "IDECDROM.hh"
#include "DeviceConfig.hh"
#include "MSXException.hh"
#include "memory.hh"

using std::unique_ptr;

namespace openmsx {
namespace IDEDeviceFactory {

unique_ptr<IDEDevice> create(const DeviceConfig& config)
{
	if (!config.getXML()) {
		return make_unique<DummyIDEDevice>();
	}
	const std::string& type = config.getChildData("type");
	if (type == "IDEHD") {
		return make_unique<IDEHD>(config);
	} else if (type == "IDECDROM") {
		return make_unique<IDECDROM>(config);
	}
	throw MSXException("Unknown IDE device: " + type);
}

} // namespace IDEDeviceFactory
} // namespace openmsx
