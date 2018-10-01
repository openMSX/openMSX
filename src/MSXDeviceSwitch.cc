#include "MSXDeviceSwitch.hh"
#include "MSXSwitchedDevice.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

MSXDeviceSwitch::MSXDeviceSwitch(const DeviceConfig& config)
	: MSXDevice(config)
{
	for (auto& dev : devices) {
		dev = nullptr;
	}
	count = 0;
	selected = 0;
}

MSXDeviceSwitch::~MSXDeviceSwitch()
{
	for (auto& dev : devices) {
		// all devices must be unregistered
		assert(!dev); (void)dev;
	}
	assert(count == 0);
}

void MSXDeviceSwitch::registerDevice(byte id, MSXSwitchedDevice* device)
{
	if (devices[id]) {
		// TODO implement multiplexing
		throw MSXException(
			"Already have a switched device with id ", int(id));
	}
	devices[id] = device;
	if (count == 0) {
		for (byte port = 0x40; port < 0x50; ++port) {
			getCPUInterface().register_IO_In (port, this);
			getCPUInterface().register_IO_Out(port, this);
		}
	}
	++count;
}

void MSXDeviceSwitch::unregisterDevice(byte id)
{
	--count;
	if (count == 0) {
		for (byte port = 0x40; port < 0x50; ++port) {
			getCPUInterface().unregister_IO_Out(port, this);
			getCPUInterface().unregister_IO_In (port, this);
		}
	}
	assert(devices[id]);
	devices[id] = nullptr;
}

void MSXDeviceSwitch::reset(EmuTime::param /*time*/)
{
	selected = 0;
}

byte MSXDeviceSwitch::readIO(word port, EmuTime::param time)
{
	if (devices[selected]) {
		return devices[selected]->readSwitchedIO(port, time);
	} else {
		return 0xFF;
	}
}

byte MSXDeviceSwitch::peekIO(word port, EmuTime::param time) const
{
	if (devices[selected]) {
		return devices[selected]->peekSwitchedIO(port, time);
	} else {
		return 0xFF;
	}
}

void MSXDeviceSwitch::writeIO(word port, byte value, EmuTime::param time)
{
	if ((port & 0x0F) == 0x00) {
		selected = value;
	} else if (devices[selected]) {
		devices[selected]->writeSwitchedIO(port, value, time);
	} else {
		// ignore
	}
}


template<typename Archive>
void MSXDeviceSwitch::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("selected", selected);
}
INSTANTIATE_SERIALIZE_METHODS(MSXDeviceSwitch);

} // namespace openmsx
