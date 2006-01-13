// $Id$

#include "MSXMultiIODevice.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

MSXMultiIODevice::MSXMultiIODevice(MSXMotherBoard& motherboard)
	: MSXMultiDevice(motherboard)
{
}

MSXMultiIODevice::~MSXMultiIODevice()
{
	assert(devices.empty());
}

void MSXMultiIODevice::addDevice(MSXDevice* device)
{
	assert(std::count(devices.begin(), devices.end(), device) == 0);
	devices.push_back(device);
}

void MSXMultiIODevice::removeDevice(MSXDevice* device)
{
	assert(std::count(devices.begin(), devices.end(), device) == 1);
	devices.erase(std::find(devices.begin(), devices.end(), device));
}

MSXMultiIODevice::Devices& MSXMultiIODevice::getDevices()
{
	return devices;
}

std::string MSXMultiIODevice::getName() const
{
	assert(!devices.empty());
	std::string name = devices[0]->getName();
	for (unsigned i = 1; i < devices.size(); ++i) {
		name += "  " + devices[i]->getName();
	}
	return name;
}

byte MSXMultiIODevice::readIO(word port, const EmuTime& time)
{
	// conflict: return the result from the first device, call readIO()
	//           also on all other devices, but discard result
	assert(!devices.empty());
	Devices::iterator it = devices.begin();
	byte result = (*it)->readIO(port, time);
	for (++it; it != devices.end(); ++it) {
		(*it)->readIO(port, time);
	}
	return result;
}

byte MSXMultiIODevice::peekIO(word port, const EmuTime& time) const
{
	// conflict: just peek first device
	assert(!devices.empty());
	return devices.front()->peekIO(port, time);
}

void MSXMultiIODevice::writeIO(word port, byte value, const EmuTime& time)
{
	for (Devices::iterator it = devices.begin();
	     it != devices.end(); ++it) {
		(*it)->writeIO(port, value, time);
	}
}

} // namespace openmsx
