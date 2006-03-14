// $Id$

#ifndef DEVICEFACTORY_HH
#define DEVICEFACTORY_HH

#include <memory>

namespace openmsx {

class MSXDevice;
class MSXMotherBoard;
class HardwareConfig;
class XMLElement;
class EmuTime;
class DummyDevice;
class MSXDeviceSwitch;
class MSXMapperIO;
class VDPIODelay;

class DeviceFactory
{
public:
	static std::auto_ptr<MSXDevice> create(
		MSXMotherBoard& motherBoard, const HardwareConfig& hwConf,
		const XMLElement& conf, const EmuTime& time);
	static std::auto_ptr<DummyDevice> createDummyDevice(
		MSXMotherBoard& motherBoard);
	static std::auto_ptr<MSXDeviceSwitch> createDeviceSwitch(
		MSXMotherBoard& motherBoard);
	static std::auto_ptr<MSXMapperIO> createMapperIO(
		MSXMotherBoard& motherBoard);
	static std::auto_ptr<VDPIODelay> createVDPIODelay(
		MSXMotherBoard& motherBoard);
};

} // namespace openmsx

#endif
