#include "MSXMultiIODevice.hh"

#include "MSXException.hh"
#include "TclObject.hh"

#include "stl.hh"

#include <cassert>

namespace openmsx {

MSXMultiIODevice::MSXMultiIODevice(HardwareConfig& hwConf)
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
	deviceName = list.getString();
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

uint8_t MSXMultiIODevice::readIO(uint16_t port, EmuTime time)
{
	// conflict: In practice, pull down seems to win over pull up,
	//           so a logical AND over the read values most accurately
	//           resembles what real hardware does.
	uint8_t result = 0xFF;
	for (auto& dev : devices) {
		result &= dev->readIO(port, time);
	}
	return result;
}

uint8_t MSXMultiIODevice::peekIO(uint16_t port, EmuTime time) const
{
	// conflict: Handle this in the same way as readIO.
	uint8_t result = 0xFF;
	for (const auto& dev : devices) {
		result &= dev->peekIO(port, time);
	}
	return result;
}

void MSXMultiIODevice::writeIO(uint16_t port, uint8_t value, EmuTime time)
{
	for (auto& dev : devices) {
		dev->writeIO(port, value, time);
	}
}

} // namespace openmsx
