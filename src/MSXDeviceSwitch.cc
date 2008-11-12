// $Id$

#include "MSXDeviceSwitch.hh"
#include "MSXSwitchedDevice.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include <cassert>

namespace openmsx {

MSXDeviceSwitch::MSXDeviceSwitch(MSXMotherBoard& motherBoard,
                                 const XMLElement& config)
	: MSXDevice(motherBoard, config)
{
	for (int i = 0; i < 256; ++i) {
		devices[i] = NULL;
	}
	count = 0;
	selected = 0;
}

MSXDeviceSwitch::~MSXDeviceSwitch()
{
	for (int i = 0; i < 256; ++i) {
		// all devices must be unregistered
		assert(devices[i] == NULL);
	}
	assert(count == 0);
}

void MSXDeviceSwitch::registerDevice(byte id, MSXSwitchedDevice* device)
{
	if (devices[id]) {
		// TODO implement multiplexing
		throw MSXException("Already have a switched device with id " +
		                   StringOp::toString(int(id)));
	}
	devices[id] = device;
	if (count == 0) {
		MSXCPUInterface& interface = getMotherBoard().getCPUInterface();
		for (byte port = 0x40; port < 0x50; ++port) {
			interface.register_IO_In (port, this);
			interface.register_IO_Out(port, this);
		}
	}
	++count;
}

void MSXDeviceSwitch::unregisterDevice(byte id)
{
	--count;
	if (count == 0) {
		MSXCPUInterface& interface = getMotherBoard().getCPUInterface();
		for (byte port = 0x40; port < 0x50; ++port) {
			interface.unregister_IO_Out(port, this);
			interface.unregister_IO_In (port, this);
		}
	}
	assert(devices[id] != NULL);
	devices[id] = NULL;
}

bool MSXDeviceSwitch::hasRegisteredDevices() const
{
	return count;
}

void MSXDeviceSwitch::reset(const EmuTime& /*time*/)
{
	selected = 0;
}

byte MSXDeviceSwitch::readIO(word port, const EmuTime& time)
{
	if (devices[selected]) {
		return devices[selected]->readSwitchedIO(port, time);
	} else {
		return 0xFF;
	}
}

byte MSXDeviceSwitch::peekIO(word port, const EmuTime& time) const
{
	if (devices[selected]) {
		return devices[selected]->peekSwitchedIO(port, time);
	} else {
		return 0xFF;
	}
}

void MSXDeviceSwitch::writeIO(word port, byte value, const EmuTime& time)
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
