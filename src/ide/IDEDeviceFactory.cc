#include "IDEDeviceFactory.hh"
#include "DummyIDEDevice.hh"
#include "IDEHD.hh"
#include "IDECDROM.hh"
#include "DeviceConfig.hh"
#include "MSXException.hh"
#include <memory>

namespace openmsx::IDEDeviceFactory {

std::unique_ptr<IDEDevice> create(const DeviceConfig& config)
{
	if (!config.getXML()) {
		return std::make_unique<DummyIDEDevice>();
	}
	const auto& type = config.getChildData("type");
	if (type == "IDEHD") {
		return std::make_unique<IDEHD>(config);
	} else if (type == "IDECDROM") {
		return std::make_unique<IDECDROM>(config);
	}
	throw MSXException("Unknown IDE device: ", type);
}

} // namespace openmsx::IDEDeviceFactory
