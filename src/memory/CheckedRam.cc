// $Id$

#include "CheckedRam.hh"
#include "Ram.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "DeviceConfig.hh"
#include "Reactor.hh"
#include "GlobalSettings.hh"
#include "StringSetting.hh"
#include "TclCallback.hh"
#include "likely.hh"
#include <cassert>

namespace openmsx {

static std::bitset<CacheLine::SIZE> getBitSetAllTrue()
{
	std::bitset<CacheLine::SIZE> result;
	result.set();
	return result;
}

CheckedRam::CheckedRam(const DeviceConfig& config, const std::string& name,
                       const std::string& description, unsigned size)
	: completely_initialized_cacheline(size / CacheLine::SIZE, false)
	, uninitialized(size / CacheLine::SIZE, getBitSetAllTrue())
	, ram(new Ram(config, name, description, size))
	, msxcpu(config.getMotherBoard().getCPU())
	, umrCallback(new TclCallback(
		config.getGlobalSettings().getUMRCallBackSetting()))
{
	umrCallback->getSetting().attach(*this);
	init();
}

CheckedRam::~CheckedRam()
{
	umrCallback->getSetting().detach(*this);
}

byte CheckedRam::read(unsigned addr)
{
	unsigned line = addr >> CacheLine::BITS;
	if (unlikely(!completely_initialized_cacheline[line])) {
		if (unlikely(uninitialized[line][addr &  CacheLine::LOW])) {
			umrCallback->execute(addr, ram->getName());
		}
	}
	return (*ram)[addr];
}

const byte* CheckedRam::getReadCacheLine(unsigned addr) const
{
	return (completely_initialized_cacheline[addr >> CacheLine::BITS])
	     ? &(*ram)[addr] : NULL;
}

byte* CheckedRam::getWriteCacheLine(unsigned addr) const
{
	return (completely_initialized_cacheline[addr >> CacheLine::BITS])
	     ? &(*ram)[addr] : NULL;
}

byte CheckedRam::peek(unsigned addr) const
{
	return (*ram)[addr];
}

void CheckedRam::write(unsigned addr, const byte value)
{
	unsigned line = addr >> CacheLine::BITS;
	if (unlikely(!completely_initialized_cacheline[line])) {
		uninitialized[line][addr & CacheLine::LOW] = false;
		if (unlikely(uninitialized[line].none())) {
			completely_initialized_cacheline[line] = true;
			msxcpu.invalidateMemCache(addr & CacheLine::HIGH,
			                          CacheLine::SIZE);
		}
	}
	(*ram)[addr] = value;
}

void CheckedRam::clear()
{
	ram->clear();
	init();
}

void CheckedRam::init()
{
	if (umrCallback->getValue().empty()) {
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
	msxcpu.invalidateMemCache(0, 0x10000);
}

unsigned CheckedRam::getSize() const
{
	return ram->getSize();
}

Ram& CheckedRam::getUncheckedRam() const
{
	return *ram;
}

void CheckedRam::update(const Setting& setting)
{
	assert(&setting == &umrCallback->getSetting());
	(void)setting;
	init();
}

} // namespace openmsx
