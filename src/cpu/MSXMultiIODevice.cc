// $Id$

#include <algorithm>
#include <cassert>
#include "EmuTime.hh"
#include "MSXMultiIODevice.hh"

using std::remove;


namespace openmsx {

MSXMultiIODevice::MSXMultiIODevice()
	: MSXDevice(NULL, EmuTime::zero), MSXIODevice(NULL, EmuTime::zero)
{
}

MSXMultiIODevice::~MSXMultiIODevice()
{
	//assert(devices.empty());
}


void MSXMultiIODevice::addDevice(MSXIODevice* device)
{
	devices.push_back(device);
	preCalcName();
}

void MSXMultiIODevice::removeDevice(MSXIODevice* device)
{
	devices.erase(remove(devices.begin(), devices.end(), device),
	              devices.end());
	preCalcName();
}

unsigned MSXMultiIODevice::numDevices() const
{
	return devices.size();
}

void MSXMultiIODevice::preCalcName()
{
	// getName() is not timing critical, but it must return a reference,
	// so we do need to store the name. So we can as well precalculate it
	name.clear();
	bool first = true;
	for (vector<MSXIODevice*>::const_iterator it = devices.begin();
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
	for (vector<MSXIODevice*>::iterator it = devices.begin();
	     it != devices.end(); ++it) {
		(*it)->reset(time);
	}
}

void MSXMultiIODevice::reInit(const EmuTime& time)
{
	for (vector<MSXIODevice*>::iterator it = devices.begin();
	     it != devices.end(); ++it) {
		(*it)->reInit(time);
	}
}

const string& MSXMultiIODevice::getName() const
{
	return name;
}


// MSXIODevice

byte MSXMultiIODevice::readIO(byte port, const EmuTime& time)
{
	// conflict: return the result from the first device, call readIO()
	//           also on all other devices, but discard result
	assert(!devices.empty());
	vector<MSXIODevice*>::iterator it = devices.begin();
	byte result = (*it)->readIO(port, time);
	for (++it; it != devices.end(); ++it) {
		(*it)->readIO(port, time);
	}
	return result;
}

void MSXMultiIODevice::writeIO(byte port, byte value, const EmuTime& time)
{
	for (vector<MSXIODevice*>::iterator it = devices.begin();
	     it != devices.end(); ++it) {
		(*it)->writeIO(port, value, time);
	}
}

} // namespace openmsx
