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
	static std::unique_ptr<MSXDevice> create(const DeviceConfig& conf);
	static std::unique_ptr<DummyDevice> createDummyDevice(
		const HardwareConfig& hcConf);
	static std::unique_ptr<MSXDeviceSwitch> createDeviceSwitch(
		const HardwareConfig& hcConf);
	static std::unique_ptr<MSXMapperIO> createMapperIO(
		const HardwareConfig& hcConf);
	static std::unique_ptr<VDPIODelay> createVDPIODelay(
		const HardwareConfig& hcConf, MSXCPUInterface& cpuInterface);
};

} // namespace openmsx

#endif
