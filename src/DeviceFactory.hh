// $Id$

#ifndef DEVICEFACTORY_HH
#define DEVICEFACTORY_HH

#include <memory>

namespace openmsx {

class MSXDevice;
class MSXMotherBoard;
class MSXCPUInterface;
class DeviceConfig;
class DummyDevice;
class MSXDeviceSwitch;
class MSXMapperIO;
class VDPIODelay;

class DeviceFactory
{
public:
	static std::auto_ptr<MSXDevice> create(
		MSXMotherBoard& motherBoard, const DeviceConfig& conf);
	static std::auto_ptr<DummyDevice> createDummyDevice(
		MSXMotherBoard& motherBoard);
	static std::auto_ptr<MSXDeviceSwitch> createDeviceSwitch(
		MSXMotherBoard& motherBoard);
	static std::auto_ptr<MSXMapperIO> createMapperIO(
		MSXMotherBoard& motherBoard);
	static std::auto_ptr<VDPIODelay> createVDPIODelay(
		MSXMotherBoard& motherBoard, MSXCPUInterface& cpuInterface);
};

} // namespace openmsx

#endif
