#include "MSXMultiIODevice.hh"
#include "MSXException.hh"
#include "TclObject.hh"
#include "stl.hh"
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
	if (contains(devices, device)) {
		throw MSXException(
			"Overlapping IO-port ranges for \"",
			device->getName(), "\".");
	}
	devices.push_back(device);
}

void MSXMultiIODevice::removeDevice(MSXDevice* device)
{
	devices.erase(rfind_unguarded(devices, device));
}

const std::string& MSXMultiIODevice::getName() const
{
	TclObject list;
	getNameList(list);
	const_cast<std::string&>(deviceName) = list.getString();
	return deviceName;
}
void MSXMultiIODevice::getNameList(TclObject& result) const
{
	for (const auto* dev : devices) {
		const auto& name = dev->getName();
		if (!name.empty()) {
			result.addListElement(name);
		}
	}
}

byte MSXMultiIODevice::readIO(word port, EmuTime::param time)
{
	// conflict: In practice, pull down seems to win over pull up,
	//           so a logical AND over the read values most accurately
	//           resembles what real hardware does.
	byte result = 0xFF;
	for (auto& dev : devices) {
		result &= dev->readIO(port, time);
	}
	return result;
}

byte MSXMultiIODevice::peekIO(word port, EmuTime::param time) const
{
	// conflict: Handle this in the same way as readIO.
	byte result = 0xFF;
	for (const auto& dev : devices) {
		result &= dev->peekIO(port, time);
	}
	return result;
}

void MSXMultiIODevice::writeIO(word port, byte value, EmuTime::param time)
{
	for (auto& dev : devices) {
		dev->writeIO(port, value, time);
	}
}

} // namespace openmsx
