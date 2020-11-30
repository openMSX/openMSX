#ifndef DEVICEFACTORY_HH
#define DEVICEFACTORY_HH

#include <memory>

namespace openmsx {

class MSXDevice;
class DeviceConfig;
class HardwareConfig;
class DummyDevice;
class MSXDeviceSwitch;
class MSXMapperIO;
class VDPIODelay;
class MSXCPUInterface;

class DeviceFactory
{
public:
	[[nodiscard]] static std::unique_ptr<MSXDevice> create(const DeviceConfig& conf);
	[[nodiscard]] static std::unique_ptr<DummyDevice> createDummyDevice(
		const HardwareConfig& hwConf);
	[[nodiscard]] static std::unique_ptr<MSXDeviceSwitch> createDeviceSwitch(
		const HardwareConfig& hwConf);
	[[nodiscard]] static std::unique_ptr<MSXMapperIO> createMapperIO(
		const HardwareConfig& hwConf);
	[[nodiscard]] static std::unique_ptr<VDPIODelay> createVDPIODelay(
		const HardwareConfig& hwConf, MSXCPUInterface& cpuInterface);
};

} // namespace openmsx

#endif
