// $Id$

#include "MSXMultiMemDevice.hh"
#include "DummyDevice.hh"
#include "MSXMotherBoard.hh"
#include "EmuTime.hh"
#include "FileContext.hh"
#include "XMLElement.hh"
#include <algorithm>
#include <cassert>

using std::remove;
using std::string;

namespace openmsx {

MSXMultiMemDevice::Range::Range(
		unsigned base_, unsigned size_, MSXDevice& device_)
	: base(base_), size(size_), device(&device_)
{
}

bool MSXMultiMemDevice::Range::operator==(const Range& other) const
{
	return (base   == other.base) &&
	       (size   == other.size) &&
	       (device == other.device);
}


static const XMLElement& getMultiConfig()
{
	static XMLElement deviceElem("MultiMem");
	static bool init = false;
	if (!init) {
		init = true;
		deviceElem.setFileContext(std::auto_ptr<FileContext>(
		                                 new SystemFileContext()));
	}
	return deviceElem;
}

MSXMultiMemDevice::MSXMultiMemDevice(MSXMotherBoard& motherboard)
	: MSXDevice(motherboard, getMultiConfig(), EmuTime::zero)
{
	// add sentinel at the end
	ranges.push_back(Range(0x0000, 0x10000, motherboard.getDummyDevice()));
}

MSXMultiMemDevice::~MSXMultiMemDevice()
{
	assert(empty());
}

static bool isInside(unsigned x, unsigned start, unsigned size)
{
	return (x - start) < size;
}

static bool overlap(unsigned start1, unsigned size1,
                    unsigned start2, unsigned size2)
{
	return (isInside(start1,             start2, size2)) ||
	       (isInside(start1 + size1 - 1, start2, size2));
}

bool MSXMultiMemDevice::add(MSXDevice& device, int base, int size)
{
	for (unsigned i = 0; i < (ranges.size() -1); ++i) {
		if (overlap(base, size, ranges[i].base, ranges[i].size)) {
			return false;
		}
	}
	ranges.insert(ranges.begin(), Range(base, size, device));
	preCalcName();
	return true;
}

void MSXMultiMemDevice::remove(MSXDevice& device, int base, int size)
{
	Ranges::iterator it = find(ranges.begin(), ranges.end(),
	                           Range(base, size, device));
	assert(it != ranges.end());
	ranges.erase(it);
	preCalcName();
}

bool MSXMultiMemDevice::empty() const
{
	return ranges.size() == 1;
}

void MSXMultiMemDevice::preCalcName()
{
	// getName() is not timing critical, but it must return a reference,
	// so we do need to store the name. So we can as well precalculate it
	name.clear();
	bool first = true;
	for (unsigned i = 0; i < (ranges.size() -1); ++i) {
		if (!first) name += "  ";
		first = false;
		name += ranges[i].device->getName();
	}
}


// MSXDevice

void MSXMultiMemDevice::reset(const EmuTime& time)
{
	for (unsigned i = 0; i < (ranges.size() -1); ++i) {
		ranges[i].device->reset(time);
	}
}

void MSXMultiMemDevice::powerDown(const EmuTime& time)
{
	for (unsigned i = 0; i < (ranges.size() -1); ++i) {
		ranges[i].device->powerDown(time);
	}
}

void MSXMultiMemDevice::powerUp(const EmuTime& time)
{
	for (unsigned i = 0; i < (ranges.size() -1); ++i) {
		ranges[i].device->powerUp(time);
	}
}

const string& MSXMultiMemDevice::getName() const
{
	return name;
}

MSXDevice* MSXMultiMemDevice::searchDevice(unsigned address)
{
	Ranges::const_iterator it = ranges.begin();
	while (true) {
		if (isInside(address, it->base, it->size)) {
			return it->device;
		}
		++it;
		assert(it != ranges.end());
	}
}

const MSXDevice* MSXMultiMemDevice::searchDevice(unsigned address) const
{
	return const_cast<MSXMultiMemDevice*>(this)->searchDevice(address);
}

byte MSXMultiMemDevice::readMem(word address, const EmuTime& time)
{
	return searchDevice(address)->readMem(address, time);
}

void MSXMultiMemDevice::writeMem(word address, byte value, const EmuTime& time)
{
	searchDevice(address)->writeMem(address, value, time);
}

const byte* MSXMultiMemDevice::getReadCacheLine(word start) const
{
	return searchDevice(start)->getReadCacheLine(start);
}

byte* MSXMultiMemDevice::getWriteCacheLine(word start) const
{
	return searchDevice(start)->getWriteCacheLine(start);
}

byte MSXMultiMemDevice::peekMem(word address, const EmuTime& time) const
{
	return searchDevice(address)->peekMem(address, time);
}

} // namespace openmsx
