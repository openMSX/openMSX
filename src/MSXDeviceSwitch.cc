// $Id$

#include "MSXDeviceSwitch.hh"
#include "EmuTime.hh"
#include "XMLElement.hh"
#include "FileContext.hh"
#include "MSXCPUInterface.hh"

namespace openmsx {

/// class MSXSwitchedDevice ///

MSXSwitchedDevice::MSXSwitchedDevice(byte id_)
	: id(id_)
{
	MSXDeviceSwitch::instance().registerDevice(id, this);
}

MSXSwitchedDevice::~MSXSwitchedDevice()
{
	MSXDeviceSwitch::instance().unregisterDevice(id);
}

void MSXSwitchedDevice::reset(const EmuTime& /*time*/)
{
}


/// class MSXDeviceSwitch ///

MSXDeviceSwitch::MSXDeviceSwitch(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
{
	for (int i = 0; i < 256; ++i) {
		devices[i] = NULL;
	}
	selected = 0;
	
	for (byte port = 0x40; port < 0x50; ++port) {
		MSXCPUInterface::instance().register_IO_In (port, this);
		MSXCPUInterface::instance().register_IO_Out(port, this);
	}
}

MSXDeviceSwitch::~MSXDeviceSwitch()
{
	for (byte port = 0x40; port < 0x50; ++port) {
		MSXCPUInterface::instance().unregister_IO_Out(port, this);
		MSXCPUInterface::instance().unregister_IO_In (port, this);
	}
	/*
	for (int i = 0; i < 256; i++) {
		// all devices must be unregistered
		assert(devices[i] == NULL);
	}
	*/
}


MSXDeviceSwitch& MSXDeviceSwitch::instance()
{
	static XMLElement config("DeviceSwitch");
	static bool init = false;
	if (!init) {
		init = true;
		config.addAttribute("id", "DeviceSwitch");
		config.setFileContext(std::auto_ptr<FileContext>(
		                                  new SystemFileContext()));
	}
	static MSXDeviceSwitch oneInstance(config, EmuTime::zero);
	return oneInstance;
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


void MSXDeviceSwitch::reset(const EmuTime& time)
{
	for (int i = 0; i < 256; i++) {
		if (devices[i]) {
			devices[i]->reset(time);
		}
	}

	selected = 0;
}

byte MSXDeviceSwitch::readIO(byte port, const EmuTime& time)
{
	if (devices[selected]) {
		//PRT_DEBUG("Switch read device " << (int)selected << " port " << (int)port);
		return devices[selected]->readIO(port, time);
	} else {
		//PRT_DEBUG("Switch read no device");
		return 0xFF;
	}
}

byte MSXDeviceSwitch::peekIO(byte port, const EmuTime& time) const
{
	if (devices[selected]) {
		return devices[selected]->peekIO(port, time);
	} else {
		return 0xFF;
	}
}

void MSXDeviceSwitch::writeIO(byte port, byte value, const EmuTime& time)
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
