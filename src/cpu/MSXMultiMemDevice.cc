// $Id$

#include "MSXMultiMemDevice.hh"
#include "DummyDevice.hh"
#include "MSXMotherBoard.hh"
#include <algorithm>
#include <cassert>

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


MSXMultiMemDevice::MSXMultiMemDevice(MSXMotherBoard& motherboard)
	: MSXMultiDevice(motherboard)
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
	return true;
}

void MSXMultiMemDevice::remove(MSXDevice& device, int base, int size)
{
	Ranges::iterator it = find(ranges.begin(), ranges.end(),
	                           Range(base, size, device));
	assert(it != ranges.end());
	ranges.erase(it);
}

bool MSXMultiMemDevice::empty() const
{
	return ranges.size() == 1;
}

std::string MSXMultiMemDevice::getName() const
{
	assert(!empty());
	std::string name = ranges[0].device->getName();
	for (unsigned i = 1; i < (ranges.size() - 1); ++i) {
		name += "  " + ranges[i].device->getName();
	}
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
