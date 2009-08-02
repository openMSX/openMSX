// $Id$

#include "CheckedRam.hh"
#include "Ram.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "CliComm.hh"
#include "GlobalSettings.hh"
#include "StringSetting.hh"
#include "TclObject.hh"
#include "likely.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

static std::bitset<CacheLine::SIZE> getBitSetAllTrue()
{
	std::bitset<CacheLine::SIZE> result;
	result.set();
	return result;
}

CheckedRam::CheckedRam(MSXMotherBoard& motherBoard, const std::string& name,
                       const std::string& description, unsigned size)
	: completely_initialized_cacheline(size / CacheLine::SIZE, false)
	, uninitialized(size / CacheLine::SIZE, getBitSetAllTrue())
	, ram(new Ram(motherBoard, name, description, size))
	, msxcpu(motherBoard.getCPU())
	, commandController(motherBoard.getCommandController())
	, umrCallbackSetting(motherBoard.getReactor().getGlobalSettings().getUMRCallBackSetting())
{
	umrCallbackSetting.attach(*this);
	init();
}

CheckedRam::~CheckedRam()
{
	umrCallbackSetting.detach(*this);
}

byte CheckedRam::read(unsigned addr)
{
	if (unlikely(uninitialized[addr >> CacheLine::BITS]
	                          [addr &  CacheLine::LOW])) {
		callUMRCallBack(addr);
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
	uninitialized[line][addr & CacheLine::LOW] = false;
	if (unlikely(uninitialized[line].none())) {
		completely_initialized_cacheline[line] = true;
		msxcpu.invalidateMemCache(addr & CacheLine::HIGH,
		                          CacheLine::SIZE);
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
	if (umrCallbackSetting.getValue().empty()) {
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

void CheckedRam::callUMRCallBack(unsigned addr)
{
	const std::string callback = umrCallbackSetting.getValue();
	if (!callback.empty()) {
		TclObject command(commandController.getInterpreter());
		command.addListElement(callback);
		command.addListElement(int(addr));
		command.addListElement(ram.get()->getName());
		try {
			command.executeCommand();
		} catch (CommandException& e) {
			commandController.getCliComm().printWarning(
				"Error executing UMR callback function \"" +
				callback + "\": " + e.getMessage() + "\n"
				"Please fix your script or set a proper "
				"callback function in the umr_callback setting.");
		}
	}
}

void CheckedRam::update(const Setting& setting)
{
	assert(&setting == &umrCallbackSetting);
	(void)setting;
	init();
}

} // namespace openmsx
