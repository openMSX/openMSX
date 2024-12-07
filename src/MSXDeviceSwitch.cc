#include "MSXDeviceSwitch.hh"
#include "MSXSwitchedDevice.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {

MSXDeviceSwitch::MSXDeviceSwitch(const DeviceConfig& config)
	: MSXDevice(config)
{
	ranges::fill(devices, nullptr);
}

MSXDeviceSwitch::~MSXDeviceSwitch()
{
	// all devices must be unregistered
	assert(ranges::all_of(devices, [](auto* dev) { return dev == nullptr; }));
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
		getCPUInterface().register_IO_InOut_range(0x40, 16, this);
	}
	++count;
}

void MSXDeviceSwitch::unregisterDevice(byte id)
{
	--count;
	if (count == 0) {
		getCPUInterface().unregister_IO_InOut_range(0x40, 16, this);
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
