#include "MSXDevice.hh"
#include "XMLElement.hh"
#include "MSXMotherBoard.hh"
#include "HardwareConfig.hh"
#include "CartridgeSlotManager.hh"
#include "MSXCPUInterface.hh"
#include "CacheLine.hh"
#include "TclObject.hh"
#include "MSXException.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <bit>
#include <cassert>

namespace openmsx {

MSXDevice::MSXDevice(const DeviceConfig& config, std::string_view name)
	: deviceConfig(config)
{
	initName(name);
}

MSXDevice::MSXDevice(const DeviceConfig& config)
	: deviceConfig(config)
{
	initName(getDeviceConfig().getAttributeValue("id"));
}

void MSXDevice::initName(std::string_view name)
{
	deviceName = name;
	if (getMotherBoard().findDevice(deviceName)) {
		unsigned n = 0;
		do {
			deviceName = strCat(name, " (", ++n, ')');
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

	ranges::fill(unmappedRead, 0xFF);
}

MSXMotherBoard& MSXDevice::getMotherBoard() const
{
	return getHardwareConfig().getMotherBoard();
}

void MSXDevice::testRemove(std::span<const std::unique_ptr<MSXDevice>> removed) const
{
	// Typically 'referencedBy' contains very few elements, so a simple
	// O(n*m) algorithm is fine.
	std::string err;
	for (const auto* dev : referencedBy) {
		if (!contains(removed, dev, [](const auto& d) { return d.get(); })) {
			strAppend(err, ' ', dev->getName());
		}
	}
	if (!err.empty()) {
		throw MSXException("Still in use by:", err);
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
	for (const auto* c : getDeviceConfig().getChildren("device")) {
		auto name = c->getAttributeValue("idref");
		auto* dev = getMotherBoard().findDevice(name);
		if (!dev) {
			throw MSXException(
				"Unsatisfied dependency: '", getName(),
				"' depends on unavailable device '",
				name, "'.");
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
MSXCliComm& MSXDevice::getCliComm() const
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
	unsigned align = getBaseSizeAlignment();
	assert(std::has_single_bit(align));
	for (const auto* m : getDeviceConfig().getChildren("mem")) {
		unsigned base = m->getAttributeValueAsInt("base", 0);
		unsigned size = m->getAttributeValueAsInt("size", 0);
		if ((base >= 0x10000) || (size > 0x10000) || ((base + size) > 0x10000)) {
			throw MSXException(
				"Invalid memory specification for device ",
				getName(), " should be in range "
				"[0x0000,0x10000).");
		}
		if (((base & (align - 1)) || (size & (align - 1))) &&
		    !allowUnaligned()) {
			throw MSXException(
				"invalid memory specification for device ",
				getName(), " should be aligned on at least 0x",
				hex_string<4>(align), '.');
		}
		tmpMemRegions.emplace_back(BaseSize{base, size});
	}
	if (tmpMemRegions.empty()) {
		return;
	}

	// find primary and secondary slot specification
	auto& slotManager = getMotherBoard().getSlotManager();
	auto* primaryConfig   = getDeviceConfig2().getPrimary();
	auto* secondaryConfig = getDeviceConfig2().getSecondary();
	if (primaryConfig) {
		ps = slotManager.getSlotNum(primaryConfig->getAttributeValue("slot"));
	} else {
		throw MSXException("Invalid memory specification");
	}
	if (secondaryConfig) {
		auto ss_str = secondaryConfig->getAttributeValue("slot");
		ss = slotManager.getSlotNum(ss_str);
		if ((-16 <= ss) && (ss <= -1) && (ss != ps)) {
			throw MSXException(
				"Invalid secondary slot specification: \"",
				ss_str, "\".");
		}
	} else {
		ss = 0;
	}

	// This is only for backwards compatibility: in the past we added extra
	// attributes "primary_slot" and "secondary_slot" (in each MSXDevice
	// config) instead of changing the 'any' value of the slot attribute of
	// the (possibly shared) <primary> and <secondary> tags. When loading
	// an old savestate these tags can still occur, so keep this code. Also
	// remove these attributes to convert to the new format.
	auto& mutableConfig = const_cast<XMLElement&>(getDeviceConfig());
	if (auto** primSlotPtr = mutableConfig.findAttributePointer("primary_slot")) {
		ps = slotManager.getSlotNum((*primSlotPtr)->getValue());
		mutableConfig.removeAttribute(primSlotPtr);
		if (auto** secSlotPtr = mutableConfig.findAttributePointer("secondary_slot")) {
			ss = slotManager.getSlotNum((*secSlotPtr)->getValue());
			mutableConfig.removeAttribute(secSlotPtr);
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
	XMLDocument& doc = deviceConfig.getXMLDocument();
	doc.setAttribute(*primaryConfig, "slot", doc.allocateString(tmpStrCat(ps)));
	if (secondaryConfig) {
		doc.setAttribute(*secondaryConfig, "slot", doc.allocateString(
			(ss == -1) ? std::string_view("X") : tmpStrCat(ss)));
	} else {
		if (ss != -1) {
			throw MSXException(
				"Missing <secondary> tag for device", getName());
		}
	}

	int logicalSS = (ss == -1) ? 0 : ss;
	for (const auto& r : tmpMemRegions) {
		getCPUInterface().registerMemDevice(
			*this, ps, logicalSS, r.base, r.size);
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
	for (const auto& [base, size] : memRegions) {
		getCPUInterface().unregisterMemDevice(
			*this, ps, logicalSS, base, size);
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

	auto lowest  = min_value(memRegions, &BaseSize::base);
	auto highest = max_value(memRegions, &BaseSize::end);
	assert(lowest <= highest);
	base = lowest;
	size = highest - lowest;
}

void MSXDevice::registerPorts()
{
	// First calculate 'inPort' and 'outPorts' ..
	for (const auto* i : getDeviceConfig().getChildren("io")) {
		unsigned base = i->getAttributeValueAsInt("base", 0);
		unsigned num  = i->getAttributeValueAsInt("num", 0);
		auto type = i->getAttributeValue("type", "IO");
		if (((base + num) > 256) || (num == 0) ||
		    (type != one_of("I", "O", "IO"))) {
			throw MSXException("Invalid IO port specification");
		}
		if (type != "O") { // "I" or "IO"
			inPorts.setPosN(base, num);
		}
		if (type != "I") { // "O" or "IO"
			outPorts.setPosN(base, num);
		}
	}
	// .. and only then register the ports. This filters possible overlaps.
	inPorts.foreachSetBit([&](auto port) {
		getCPUInterface().register_IO_In(narrow_cast<byte>(port), this);
	});
	outPorts.foreachSetBit([&](auto port) {
		getCPUInterface().register_IO_Out(narrow_cast<byte>(port), this);
	});
}

void MSXDevice::unregisterPorts()
{
	inPorts.foreachSetBit([&](auto port) {
		getCPUInterface().unregister_IO_In(narrow_cast<byte>(port), this);
	});
	outPorts.foreachSetBit([&](auto port) {
		getCPUInterface().unregister_IO_Out(narrow_cast<byte>(port), this);
	});
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

const std::string& MSXDevice::getName() const
{
	return deviceName;
}

void MSXDevice::getNameList(TclObject& result) const
{
	result.addListElement(getName());
}

void MSXDevice::getDeviceInfo(TclObject& result) const
{
	result.addDictKeyValue("type", getDeviceConfig().getName());
	getExtraDeviceInfo(result);
}

void MSXDevice::getExtraDeviceInfo(TclObject& /*result*/) const
{
	// nothing
}

unsigned MSXDevice::getBaseSizeAlignment() const
{
	return CacheLine::SIZE;
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

void MSXDevice::globalRead(word /*address*/, EmuTime::param /*time*/)
{
	UNREACHABLE;
}

byte* MSXDevice::getWriteCacheLine(word /*start*/)
{
	return nullptr; // uncacheable
}


// calls 'action(<start2>, <size2>, args..., ps, ss)'
// with 'start', 'size' clipped to each of the ranges in 'memRegions'
template<typename Action, typename... Args>
void MSXDevice::clip(unsigned start, unsigned size, Action action, Args... args)
{
	assert(start < 0x10000);
	int ss2 = (ss != -1) ? ss : 0;
	unsigned end = start + size;
	for (auto [base, fullBsize] : memRegions) {
		// split on 16kB boundaries
		while (fullBsize > 0) {
			unsigned bsize = std::min(fullBsize, ((base + 0x4000) & ~0x3fff) - base);

			unsigned baseEnd = base + bsize;
			// intersect [start, end) with [base, baseEnd) -> [clipStart, clipEnd)
			unsigned clipStart = std::max(start, base);
			unsigned clipEnd   = std::min(end, baseEnd);
			if (clipStart < clipEnd) { // non-empty
				unsigned clipSize = clipEnd - clipStart;
				action(narrow<word>(clipStart), clipSize, args..., ps, ss2);
			}

			base += bsize;
			fullBsize -= bsize;
		}
	}
}

void MSXDevice::invalidateDeviceRWCache(unsigned start, unsigned size)
{
	clip(start, size, [&](auto... args) { getCPUInterface().invalidateRWCache(args...); });
}
void MSXDevice::invalidateDeviceRCache(unsigned start, unsigned size)
{
	clip(start, size, [&](auto... args) { getCPUInterface().invalidateRCache(args...); });
}
void MSXDevice::invalidateDeviceWCache(unsigned start, unsigned size)
{
	clip(start, size, [&](auto... args) { getCPUInterface().invalidateWCache(args...); });
}

void MSXDevice::fillDeviceRWCache(unsigned start, unsigned size, byte* rwData)
{
	fillDeviceRWCache(start, size, rwData, rwData);
}
void MSXDevice::fillDeviceRWCache(unsigned start, unsigned size, const byte* rData, byte* wData)
{
	assert(!allowUnaligned());
	clip(start, size, [&](auto... args) { getCPUInterface().fillRWCache(args...); }, rData, wData);
}
void MSXDevice::fillDeviceRCache(unsigned start, unsigned size, const byte* rData)
{
	assert(!allowUnaligned());
	clip(start, size, [&](auto... args) { getCPUInterface().fillRCache(args...); }, rData);
}
void MSXDevice::fillDeviceWCache(unsigned start, unsigned size, byte* wData)
{
	assert(!allowUnaligned());
	clip(start, size, [&](auto... args) { getCPUInterface().fillWCache(args...); }, wData);
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
