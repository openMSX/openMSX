#include "MSXDevice.hh"
#include "XMLElement.hh"
#include "MSXMotherBoard.hh"
#include "HardwareConfig.hh"
#include "CartridgeSlotManager.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "CacheLine.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "stl.hh"
#include "unreachable.hh"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iterator> // for back_inserter

using std::string;
using std::vector;

namespace openmsx {

byte MSXDevice::unmappedRead[0x10000];
byte MSXDevice::unmappedWrite[0x10000];


MSXDevice::MSXDevice(const DeviceConfig& config, const string& name)
	: deviceConfig(config)
{
	initName(name);
}

MSXDevice::MSXDevice(const DeviceConfig& config)
	: deviceConfig(config)
{
	initName(getDeviceConfig().getAttribute("id"));
}

void MSXDevice::initName(const string& name)
{
	deviceName = name;
	if (getMotherBoard().findDevice(deviceName)) {
		unsigned n = 0;
		do {
			deviceName = StringOp::Builder() << name << " (" << ++n << ')';
		} while (getMotherBoard().findDevice(deviceName));
	}
}

void MSXDevice::init()
{
	staticInit();

	lockDevices();
	registerSlots();
	registerPorts();
}

MSXDevice::~MSXDevice()
{
	unregisterPorts();
	unregisterSlots();
	unlockDevices();
	assert(referencedBy.empty());
}

void MSXDevice::staticInit()
{
	static bool alreadyInit = false;
	if (alreadyInit) return;
	alreadyInit = true;

	memset(unmappedRead, 0xFF, sizeof(unmappedRead));
}

MSXMotherBoard& MSXDevice::getMotherBoard() const
{
       return getHardwareConfig().getMotherBoard();
}

void MSXDevice::testRemove(Devices removed) const
{
	auto all = referencedBy;
	sort(begin(all),     end(all));
	sort(begin(removed), end(removed));
	Devices rest;
	set_difference(begin(all), end(all), begin(removed), end(removed),
	               back_inserter(rest));
	if (!rest.empty()) {
		StringOp::Builder msg;
		msg << "Still in use by ";
		for (auto& d : rest) {
			msg << d->getName() << ' ';
		}
		throw MSXException(msg);
	}
}

void MSXDevice::lockDevices()
{
	// This code can only handle backward references: the thing that is
	// referenced must already be instantiated, we don't try to change the
	// instantiation order. For the moment this is good enough (only ADVRAM
	// (an extension) uses it to refer to the VDP (inside a machine)). If
	// needed we can implement something more sophisticated later without
	// changing the format of the config files.
	for (auto& c : getDeviceConfig().getChildren("device")) {
		const auto& name = c->getAttribute("idref");
		auto* dev = getMotherBoard().findDevice(name);
		if (!dev) {
			throw MSXException(
				"Unsatisfied dependency: '" + getName() +
				"' depends on unavailable device '" +
				name + "'.");
		}
		references.push_back(dev);
		dev->referencedBy.push_back(this);
	}
}

void MSXDevice::unlockDevices()
{
	for (auto& r : references) {
		move_pop_back(r->referencedBy, rfind_unguarded(r->referencedBy, this));
	}
}

const MSXDevice::Devices& MSXDevice::getReferences() const
{
	// init() must already be called
	return references;
}

EmuTime::param MSXDevice::getCurrentTime() const
{
	return getMotherBoard().getCurrentTime();
}
MSXCPU& MSXDevice::getCPU() const
{
	return getMotherBoard().getCPU();
}
MSXCPUInterface& MSXDevice::getCPUInterface() const
{
	return getMotherBoard().getCPUInterface();
}
Scheduler& MSXDevice::getScheduler() const
{
	return getMotherBoard().getScheduler();
}
CliComm& MSXDevice::getCliComm() const
{
	return getMotherBoard().getMSXCliComm();
}
Reactor& MSXDevice::getReactor() const
{
	return getMotherBoard().getReactor();
}
CommandController& MSXDevice::getCommandController() const
{
	return getMotherBoard().getCommandController();
}
LedStatus& MSXDevice::getLedStatus() const
{
	return getMotherBoard().getLedStatus();
}
PluggingController& MSXDevice::getPluggingController() const
{
	return getMotherBoard().getPluggingController();
}

void MSXDevice::registerSlots()
{
	MemRegions tmpMemRegions;
	for (auto& m : getDeviceConfig().getChildren("mem")) {
		unsigned base = m->getAttributeAsInt("base");
		unsigned size = m->getAttributeAsInt("size");
		if ((base >= 0x10000) || (size > 0x10000)) {
			throw MSXException(
				"Invalid memory specification for device " +
				getName() + " should be in range "
				"[0x0000,0x10000).");
		}
		tmpMemRegions.emplace_back(base, size);
	}
	if (tmpMemRegions.empty()) {
		return;
	}

	// find primary and secondary slot specification
	auto& slotManager = getMotherBoard().getSlotManager();
	auto* primaryConfig   = getDeviceConfig2().getPrimary();
	auto* secondaryConfig = getDeviceConfig2().getSecondary();
	if (primaryConfig) {
		ps = slotManager.getSlotNum(primaryConfig->getAttribute("slot"));
	} else {
		throw MSXException("Invalid memory specification");
	}
	if (secondaryConfig) {
		ss = slotManager.getSlotNum(secondaryConfig->getAttribute("slot"));
	} else {
		ss = 0;
	}

	// This is only for backwards compatibility: in the past we added extra
	// attributes "primary_slot" and "secondary_slot" (in each MSXDevice
	// config) instead of changing the 'any' value of the slot attribute of
	// the (possibly shared) <primary> and <secondary> tags. When loading
	// an old savestate these tags can still occur, so keep this code. Also
	// remove these attributes to convert to the new format.
	const auto& config = getDeviceConfig();
	if (config.hasAttribute("primary_slot")) {
		auto& mutableConfig = const_cast<XMLElement&>(config);
		const auto& primSlot = config.getAttribute("primary_slot");
		ps = slotManager.getSlotNum(primSlot);
		mutableConfig.removeAttribute("primary_slot");
		if (config.hasAttribute("secondary_slot")) {
			const auto& secondSlot = config.getAttribute("secondary_slot");
			ss = slotManager.getSlotNum(secondSlot);
			mutableConfig.removeAttribute("secondary_slot");
		}
	}

	// decode special values for 'ss'
	if ((-128 <= ss) && (ss < 0)) {
		if ((0 <= ps) && (ps < 4) &&
		    getCPUInterface().isExpanded(ps)) {
			ss += 128;
		} else {
			ss = 0;
		}
	}

	// decode special values for 'ps'
	if (ps == -256) {
		slotManager.getAnyFreeSlot(ps, ss);
	} else if (ps < 0) {
		// specified slot by name (carta, cartb, ...)
		slotManager.getSpecificSlot(-ps - 1, ps, ss);
	} else {
		// numerical specified slot (0, 1, 2, 3)
	}
	assert((0 <= ps) && (ps <= 3));

	if (!getCPUInterface().isExpanded(ps)) {
		ss = -1;
	}

	// Store actual slot in config. This has two purposes:
	//  - Make sure that devices that are grouped under the same
	//    <primary>/<secondary> tags actually use the same slot. (This
	//    matters when the value of some of the slot attributes is "any").
	//  - Fix the slot number so that it remains the same after a
	//    savestate/loadstate.
	assert(primaryConfig);
	primaryConfig->setAttribute("slot", StringOp::toString(ps));
	if (secondaryConfig) {
		string slot = (ss == -1) ? "X" : StringOp::toString(ss);
		secondaryConfig->setAttribute("slot", slot);
	} else {
		if (ss != -1) {
			throw MSXException(
				"Missing <secondary> tag for device" +
				getName());
		}
	}

	int logicalSS = (ss == -1) ? 0 : ss;
	for (auto& r : tmpMemRegions) {
		getCPUInterface().registerMemDevice(
			*this, ps, logicalSS, r.first, r.second);
		memRegions.push_back(r);
	}

	// Mark the slot as 'in-use' so that future searches for free external
	// slots don't return this slot anymore. If the slot was not an
	// external slot, this call has no effect. Multiple MSXDevices from the
	// same extension (the same HardwareConfig) can all allocate the same
	// slot (later they should also all free this slot).
	slotManager.allocateSlot(ps, ss, getHardwareConfig());
}

void MSXDevice::unregisterSlots()
{
	if (memRegions.empty()) return;

	int logicalSS = (ss == -1) ? 0 : ss;
	for (auto& r : memRegions) {
		getCPUInterface().unregisterMemDevice(
			*this, ps, logicalSS, r.first, r.second);
	}

	// See comments above about allocateSlot() for more details:
	//  - has no effect for non-external slots
	//  - can be called multiple times for the same slot
	getMotherBoard().getSlotManager().freeSlot(ps, ss, getHardwareConfig());
}

void MSXDevice::getVisibleMemRegion(unsigned& base, unsigned& size) const
{
	// init() must already be called
	if (memRegions.empty()) {
		base = 0;
		size = 0;
		return;
	}
	auto it = begin(memRegions);
	unsigned lowest  = it->first;
	unsigned highest = it->first + it->second;
	for (++it; it != end(memRegions); ++it) {
		lowest  = std::min(lowest,  it->first);
		highest = std::max(highest, it->first + it->second);
	}
	assert(lowest <= highest);
	base = lowest;
	size = highest - lowest;
}

void MSXDevice::registerPorts()
{
	for (auto& i : getDeviceConfig().getChildren("io")) {
		unsigned base = i->getAttributeAsInt("base");
		unsigned num  = i->getAttributeAsInt("num");
		const auto& type = i->getAttribute("type", "IO");
		if (((base + num) > 256) || (num == 0) ||
		    ((type != "I") && (type != "O") && (type != "IO"))) {
			throw MSXException("Invalid IO port specification");
		}
		for (unsigned port = base; port < base + num; ++port) {
			if ((type == "I") || (type == "IO")) {
				getCPUInterface().register_IO_In(port, this);
				inPorts.push_back(port);
			}
			if ((type == "O") || (type == "IO")) {
				getCPUInterface().register_IO_Out(port, this);
				outPorts.push_back(port);
			}
		}
	}
}

void MSXDevice::unregisterPorts()
{
	for (auto& p : inPorts) {
		getCPUInterface().unregister_IO_In(p, this);
	}
	for (auto& p : outPorts) {
		getCPUInterface().unregister_IO_Out(p, this);
	}
}


void MSXDevice::reset(EmuTime::param /*time*/)
{
	// nothing
}

byte MSXDevice::readIRQVector()
{
	return 0xFF;
}

void MSXDevice::powerDown(EmuTime::param /*time*/)
{
	// nothing
}

void MSXDevice::powerUp(EmuTime::param time)
{
	reset(time);
}

string MSXDevice::getName() const
{
	return deviceName;
}

void MSXDevice::getNameList(TclObject& result) const
{
	result.addListElement(getName());
}

void MSXDevice::getDeviceInfo(TclObject& result) const
{
	result.addListElement(getDeviceConfig().getName());
	getExtraDeviceInfo(result);
}

void MSXDevice::getExtraDeviceInfo(TclObject& /*result*/) const
{
	// nothing
}


byte MSXDevice::readIO(word /*port*/, EmuTime::param /*time*/)
{
	// read from unmapped IO
	return 0xFF;
}

void MSXDevice::writeIO(word /*port*/, byte /*value*/, EmuTime::param /*time*/)
{
	// write to unmapped IO, do nothing
}

byte MSXDevice::peekIO(word /*port*/, EmuTime::param /*time*/) const
{
	return 0xFF;
}


byte MSXDevice::readMem(word /*address*/, EmuTime::param /*time*/)
{
	// read from unmapped memory
	return 0xFF;
}

const byte* MSXDevice::getReadCacheLine(word /*start*/) const
{
	return nullptr; // uncacheable
}

void MSXDevice::writeMem(word /*address*/, byte /*value*/,
                         EmuTime::param /*time*/)
{
	// write to unmapped memory, do nothing
}

byte MSXDevice::peekMem(word address, EmuTime::param /*time*/) const
{
	word base = address & CacheLine::HIGH;
	if (const byte* cache = getReadCacheLine(base)) {
		word offset = address & CacheLine::LOW;
		return cache[offset];
	} else {
		// peek not supported for this device
		return 0xFF;
	}
}

void MSXDevice::globalWrite(word /*address*/, byte /*value*/,
                            EmuTime::param /*time*/)
{
	UNREACHABLE;
}

byte* MSXDevice::getWriteCacheLine(word /*start*/) const
{
	return nullptr; // uncacheable
}

void MSXDevice::invalidateMemCache(word start, unsigned size)
{
	getCPU().invalidateMemCache(start, size);
}

template<typename Archive>
void MSXDevice::serialize(Archive& ar, unsigned /*version*/)
{
	// When this method is called, the method init() has already been
	// called (thus also registerSlots() and registerPorts()).
	ar.serialize("name", deviceName);
}
INSTANTIATE_SERIALIZE_METHODS(MSXDevice);

} // namespace openmsx
