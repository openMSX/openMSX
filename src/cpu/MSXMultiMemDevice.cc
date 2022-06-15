#include "MSXMultiMemDevice.hh"
#include "DummyDevice.hh"
#include "MSXCPUInterface.hh"
#include "TclObject.hh"
#include "likely.hh"
#include "ranges.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "view.hh"
#include <cassert>

namespace openmsx {

MSXMultiMemDevice::Range::Range(
		unsigned base_, unsigned size_, MSXDevice& device_)
	: base(base_), size(size_), device(&device_)
{
}


MSXMultiMemDevice::MSXMultiMemDevice(const HardwareConfig& hwConf)
	: MSXMultiDevice(hwConf)
{
	// add sentinel at the end
	ranges.emplace_back(0x0000, 0x10000, getCPUInterface().getDummyDevice());
}

MSXMultiMemDevice::~MSXMultiMemDevice()
{
	assert(empty());
}

static constexpr bool isInside(unsigned x, unsigned start, unsigned size)
{
	return (x - start) < size;
}

static constexpr bool overlap(unsigned start1, unsigned size1,
                              unsigned start2, unsigned size2)
{
	return (isInside(start1,             start2, size2)) ||
	       (isInside(start1 + size1 - 1, start2, size2));
}

bool MSXMultiMemDevice::canAdd(int base, int size)
{
	return ranges::none_of(view::drop_back(ranges, 1), [&](auto& rn) {
		return overlap(base, size, rn.base, rn.size);
	});
}

void MSXMultiMemDevice::add(MSXDevice& device, int base, int size)
{
	assert(canAdd(base, size));
	ranges.insert(begin(ranges), Range(base, size, device));
}

void MSXMultiMemDevice::remove(MSXDevice& device, int base, int size)
{
	ranges.erase(rfind_unguarded(ranges, Range(base, size, device)));
}

std::vector<MSXDevice*> MSXMultiMemDevice::getDevices() const
{
	return to_vector(view::transform(view::drop_back(ranges, 1),
	                                 [](auto& rn) { return rn.device; }));
}

const std::string& MSXMultiMemDevice::getName() const
{
	TclObject list;
	getNameList(list);
	const_cast<std::string&>(deviceName) = list.getString();
	return deviceName;
}
void MSXMultiMemDevice::getNameList(TclObject& result) const
{
	for (const auto& r : ranges) {
		const auto& name = r.device->getName();
		if (!name.empty()) {
			result.addListElement(name);
		}
	}
}

const MSXMultiMemDevice::Range& MSXMultiMemDevice::searchRange(unsigned address) const
{
	for (const auto& r : ranges) {
		if (isInside(address, r.base, r.size)) {
			return r;
		}
	}
	UNREACHABLE; return ranges.back();
}

MSXDevice* MSXMultiMemDevice::searchDevice(unsigned address) const
{
	return searchRange(address).device;
}

byte MSXMultiMemDevice::readMem(word address, EmuTime::param time)
{
	return searchDevice(address)->readMem(address, time);
}

byte MSXMultiMemDevice::peekMem(word address, EmuTime::param time) const
{
	return searchDevice(address)->peekMem(address, time);
}

void MSXMultiMemDevice::writeMem(word address, byte value, EmuTime::param time)
{
	searchDevice(address)->writeMem(address, value, time);
}

const byte* MSXMultiMemDevice::getReadCacheLine(word start) const
{
	assert((start & CacheLine::HIGH) == start); // start is aligned
	// Because start is aligned we don't need to worry about the begin
	// address of the range. But we must make sure the end of the range
	// doesn't only fill a partial cacheline.
	const auto& range = searchRange(start);
	if (unlikely(((range.base + range.size) & CacheLine::HIGH) == start)) {
		// The end of this memory device only fills a partial
		// cacheline. This can't be cached.
		return nullptr;
	}
	return range.device->getReadCacheLine(start);
}

byte* MSXMultiMemDevice::getWriteCacheLine(word start) const
{
	assert((start & CacheLine::HIGH) == start);
	const auto& range = searchRange(start);
	if (unlikely(((range.base + range.size) & CacheLine::HIGH) == start)) {
		return nullptr;
	}
	return range.device->getWriteCacheLine(start);
}

} // namespace openmsx
