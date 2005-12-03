// $Id$

#include "MSXMultiIODevice.hh"
#include "EmuTime.hh"
#include "FileContext.hh"
#include "XMLElement.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

const XMLElement& getMultiConfig()
{
	static XMLElement deviceElem("MultiIO");
	static bool init = false;
	if (!init) {
		init = true;
		deviceElem.setFileContext(std::auto_ptr<FileContext>(
		                                 new SystemFileContext()));
	}
	return deviceElem;
}

MSXMultiIODevice::MSXMultiIODevice(MSXMotherBoard& motherboard)
	: MSXDevice(motherboard, getMultiConfig(), EmuTime::zero)
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
	preCalcName();
}

void MSXMultiIODevice::removeDevice(MSXDevice* device)
{
	assert(std::count(devices.begin(), devices.end(), device) == 1);
	devices.erase(std::find(devices.begin(), devices.end(), device));
	preCalcName();
}

MSXMultiIODevice::Devices& MSXMultiIODevice::getDevices()
{
	return devices;
}

void MSXMultiIODevice::preCalcName()
{
	// getName() is not timing critical, but it must return a reference,
	// so we do need to store the name. So we can as well precalculate it
	name.clear();
	bool first = true;
	for (Devices::const_iterator it = devices.begin();
	     it != devices.end(); ++it) {
		if (!first) {
			name += "  ";
		}
		first = false;
		name += (*it)->getName();
	}
}


// MSXDevice

void MSXMultiIODevice::reset(const EmuTime& time)
{
	for (Devices::iterator it = devices.begin();
	     it != devices.end(); ++it) {
		(*it)->reset(time);
	}
}

void MSXMultiIODevice::powerDown(const EmuTime& time)
{
	for (Devices::iterator it = devices.begin();
	     it != devices.end(); ++it) {
		(*it)->powerDown(time);
	}
}

void MSXMultiIODevice::powerUp(const EmuTime& time)
{
	for (Devices::iterator it = devices.begin();
	     it != devices.end(); ++it) {
		(*it)->powerUp(time);
	}
}

const std::string& MSXMultiIODevice::getName() const
{
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
