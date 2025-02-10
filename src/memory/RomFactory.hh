#ifndef ROMFACTORY_HH
#define ROMFACTORY_HH

#include <memory>

namespace openmsx {

class MSXDevice;
class DeviceConfig;

namespace RomFactory
{
	[[nodiscard]] std::unique_ptr<MSXDevice> create(DeviceConfig& config);
}

} // namespace openmsx

#endif
