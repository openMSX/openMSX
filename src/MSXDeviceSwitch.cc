// $Id$

#include "MSXDeviceSwitch.hh"
#include "EmuTime.hh"
#include "Device.hh"


namespace openmsx {

/// class MSXSwitchedDevice ///

MSXSwitchedDevice::MSXSwitchedDevice(byte id_)
	: id(id_)
{
	MSXDeviceSwitch::instance()->registerDevice(id, this);
}

MSXSwitchedDevice::~MSXSwitchedDevice()
{
	MSXDeviceSwitch::instance()->unregisterDevice(id);
}


/// class MSXDeviceSwitch ///

MSXDeviceSwitch::MSXDeviceSwitch(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	for (int i = 0; i < 256; i++) {
		devices[i] = NULL;
	}
	selected = 0;
}

MSXDeviceSwitch::~MSXDeviceSwitch()
{
/*
	for (int i = 0; i < 256; i++) {
		// all devices must be unregistered
		assert(devices[i] == NULL);
	}
*/
}


MSXDeviceSwitch* MSXDeviceSwitch::instance()
{
	static Device device("DeviceSwitch", "DeviceSwitch");
	static MSXDeviceSwitch oneInstance(&device, EmuTime::zero);
	return &oneInstance;
}


void MSXDeviceSwitch::registerDevice(byte id, MSXSwitchedDevice* device)
{
	//PRT_DEBUG("Switch: register device with id " << (int)id);
	assert(devices[id] == NULL);
	devices[id] = device;
}

void MSXDeviceSwitch::unregisterDevice(byte id)
{
	assert(devices[id] != NULL);
	devices[id] = NULL;
}


void MSXDeviceSwitch::reset(const EmuTime &time)
{
	for (int i = 0; i < 256; i++) {
		if (devices[i]) {
			devices[i]->reset(time);
		}
	}

	selected = 0;
}

byte MSXDeviceSwitch::readIO(byte port, const EmuTime &time)
{
	if (devices[selected]) {
		//PRT_DEBUG("Switch read device " << (int)selected << " port " << (int)port);
		return devices[selected]->readIO(port, time);
	} else {
		//PRT_DEBUG("Switch read no device");
		return 0xFF;
	}
}

void MSXDeviceSwitch::writeIO(byte port, byte value, const EmuTime &time)
{
	port &= 0x0F;
	if (port == 0x00) {
		selected = value;
		PRT_DEBUG("Switch " << (int)selected);
	} else if (devices[selected]) {
		//PRT_DEBUG("Switch write device " << (int)selected << " port " << (int)port);
		devices[selected]->writeIO(port, value, time);
	} else {
		//ignore
	}
}

} // namespace openmsx
