#include "CheckedRam.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "DeviceConfig.hh"
#include "GlobalSettings.hh"
#include "StringSetting.hh"
#include "narrow.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {

CheckedRam::CheckedRam(const DeviceConfig& config, const std::string& name,
                       static_string_view description, size_t size)
	: ram(config, name, description, size)
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

byte CheckedRam::read(size_t addr)
{
	if (size_t line = addr >> CacheLine::BITS;
	    !completely_initialized_cacheline[line]) [[unlikely]] {
		if (uninitialized[line][addr &  CacheLine::LOW]) [[unlikely]] {
			umrCallback.execute(narrow<int>(addr), ram.getName());
		}
	}
	return ram[addr];
}

const byte* CheckedRam::getReadCacheLine(size_t addr) const
{
	return (completely_initialized_cacheline[addr >> CacheLine::BITS])
	     ? &ram[addr] : nullptr;
}

byte* CheckedRam::getWriteCacheLine(size_t addr)
{
	return (completely_initialized_cacheline[addr >> CacheLine::BITS])
	     ? &ram[addr] : nullptr;
}

byte* CheckedRam::getRWCacheLines(size_t addr, size_t size)
{
	// TODO optimize
	size_t num = size >> CacheLine::BITS;
	size_t first = addr >> CacheLine::BITS;
	for (auto i : xrange(num)) {
		if (!completely_initialized_cacheline[first + i]) {
			return nullptr;
		}
	}
	return &ram[addr];
}

void CheckedRam::write(size_t addr, const byte value)
{
	if (size_t line = addr >> CacheLine::BITS;
	    !completely_initialized_cacheline[line]) [[unlikely]] {
		uninitialized[line][addr & CacheLine::LOW] = false;
		if (uninitialized[line].none()) [[unlikely]] {
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
	auto lines = ram.size() / CacheLine::SIZE;
	if (umrCallback.getValue().empty()) {
		// there is no callback function,
		// do as if everything is initialized
		completely_initialized_cacheline.assign(lines, true);
		// 'uninitialized' won't be accessed, so don't even allocate
	} else {
		// new callback function, forget about initialized areas
		completely_initialized_cacheline.assign(lines, false);

		std::bitset<CacheLine::SIZE> allTrue;
		allTrue.set();
		uninitialized.assign(lines, allTrue);
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
