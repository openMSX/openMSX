#include "CheckedRam.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "DeviceConfig.hh"
#include "GlobalSettings.hh"
#include "StringSetting.hh"
#include "likely.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {

[[nodiscard]] static std::bitset<CacheLine::SIZE> getBitSetAllTrue()
{
	std::bitset<CacheLine::SIZE> result;
	result.set();
	return result;
}

CheckedRam::CheckedRam(const DeviceConfig& config, const std::string& name,
                       static_string_view description, unsigned size)
	: completely_initialized_cacheline(size / CacheLine::SIZE, false)
	, uninitialized(size / CacheLine::SIZE, getBitSetAllTrue())
	, ram(config, name, description, size)
	, msxcpu(config.getMotherBoard().getCPU())
	, umrCallback(config.getGlobalSettings().getUMRCallBackSetting())
{
	umrCallback.getSetting().attach(*this);
	init();
}

CheckedRam::~CheckedRam()
{
	umrCallback.getSetting().detach(*this);
}

byte CheckedRam::read(unsigned addr)
{
	unsigned line = addr >> CacheLine::BITS;
	if (unlikely(!completely_initialized_cacheline[line])) {
		if (unlikely(uninitialized[line][addr &  CacheLine::LOW])) {
			umrCallback.execute(addr, ram.getName());
		}
	}
	return ram[addr];
}

const byte* CheckedRam::getReadCacheLine(unsigned addr) const
{
	return (completely_initialized_cacheline[addr >> CacheLine::BITS])
	     ? &ram[addr] : nullptr;
}

byte* CheckedRam::getWriteCacheLine(unsigned addr) const
{
	return (completely_initialized_cacheline[addr >> CacheLine::BITS])
	     ? const_cast<byte*>(&ram[addr]) : nullptr;
}

byte* CheckedRam::getRWCacheLines(unsigned addr, unsigned size) const
{
	// TODO optimize
	unsigned num = size >> CacheLine::BITS;
	unsigned first = addr >> CacheLine::BITS;
	for (auto i : xrange(num)) {
		if (!completely_initialized_cacheline[first + i]) {
			return nullptr;
		}
	}
	return const_cast<byte*>(&ram[addr]);
}

void CheckedRam::write(unsigned addr, const byte value)
{
	unsigned line = addr >> CacheLine::BITS;
	if (unlikely(!completely_initialized_cacheline[line])) {
		uninitialized[line][addr & CacheLine::LOW] = false;
		if (unlikely(uninitialized[line].none())) {
			completely_initialized_cacheline[line] = true;
			// This invalidates way too much stuff. But because
			// (locally) we don't know exactly how this class ie
			// being used in the MSXDevice, there's no easy way to
			// figure out what exactly should be invalidated.
			//
			// This is anyway only a debug feature (usually it's
			// not in use), therefor it's OK to implement this in a
			// easy/slow way rather than complex/fast.
			msxcpu.invalidateAllSlotsRWCache(0, 0x10000);
		}
	}
	ram[addr] = value;
}

void CheckedRam::clear()
{
	ram.clear();
	init();
}

void CheckedRam::init()
{
	if (umrCallback.getValue().empty()) {
		// there is no callback function,
		// do as if everything is initialized
		completely_initialized_cacheline.assign(
			completely_initialized_cacheline.size(), true);
		uninitialized.assign(
			uninitialized.size(), std::bitset<CacheLine::SIZE>());
	} else {
		// new callback function, forget about initialized areas
		completely_initialized_cacheline.assign(
			completely_initialized_cacheline.size(), false);
		uninitialized.assign(
			uninitialized.size(), getBitSetAllTrue());
	}
	msxcpu.invalidateAllSlotsRWCache(0, 0x10000);
}

void CheckedRam::update(const Setting& setting) noexcept
{
	assert(&setting == &umrCallback.getSetting());
	(void)setting;
	init();
}

} // namespace openmsx
