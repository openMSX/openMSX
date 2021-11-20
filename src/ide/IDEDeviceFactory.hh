#ifndef IDEDEVICEFACTORY_HH
#define IDEDEVICEFACTORY_HH

#include <memory>

namespace openmsx {

class IDEDevice;
class DeviceConfig;

namespace IDEDeviceFactory
{
	[[nodiscard]] std::unique_ptr<IDEDevice> create(const DeviceConfig& config);
}

} // namespace openmsx

#endif
