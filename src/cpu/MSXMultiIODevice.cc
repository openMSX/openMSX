#include "MSXMultiIODevice.hh"
#include "TclObject.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

MSXMultiIODevice::MSXMultiIODevice(const HardwareConfig& hwConf)
	: MSXMultiDevice(hwConf)
{
}

MSXMultiIODevice::~MSXMultiIODevice()
{
	assert(devices.empty());
}

void MSXMultiIODevice::addDevice(MSXDevice* device)
{
	assert(std::count(begin(devices), end(devices), device) == 0);
	devices.push_back(device);
}

void MSXMultiIODevice::removeDevice(MSXDevice* device)
{
	assert(std::count(begin(devices), end(devices), device) == 1);
	devices.erase(std::find(begin(devices), end(devices), device));
}

MSXMultiIODevice::Devices& MSXMultiIODevice::getDevices()
{
	return devices;
}

std::string MSXMultiIODevice::getName() const
{
	TclObject list;
	getNameList(list);
	return list.getString().str();
}
void MSXMultiIODevice::getNameList(TclObject& result) const
{
	for (auto* dev : devices) {
		const auto& name = dev->getName();
		if (!name.empty()) {
			result.addListElement(name);
		}
	}
}

byte MSXMultiIODevice::readIO(word port, EmuTime::param time)
{
	// conflict: return the result from the first device, call readIO()
	//           also on all other devices, but discard result
	assert(!devices.empty());
	auto it = begin(devices);
	byte result = (*it)->readIO(port, time);
	for (++it; it != end(devices); ++it) {
		(*it)->readIO(port, time);
	}
	return result;
}

byte MSXMultiIODevice::peekIO(word port, EmuTime::param time) const
{
	// conflict: just peek first device
	assert(!devices.empty());
	return devices.front()->peekIO(port, time);
}

void MSXMultiIODevice::writeIO(word port, byte value, EmuTime::param time)
{
	for (auto& d : devices) {
		d->writeIO(port, value, time);
	}
}

} // namespace openmsx
