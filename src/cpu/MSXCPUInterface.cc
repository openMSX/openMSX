#include "MSXCPUInterface.hh"
#include "DebugCondition.hh"
#include "DummyDevice.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "Interpreter.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "VDPIODelay.hh"
#include "CliComm.hh"
#include "MSXMultiIODevice.hh"
#include "MSXMultiMemDevice.hh"
#include "MSXWatchIODevice.hh"
#include "MSXException.hh"
#include "CartridgeSlotManager.hh"
#include "EventDistributor.hh"
#include "Event.hh"
#include "DeviceFactory.hh"
#include "ReadOnlySetting.hh"
#include "serialize.hh"
#include "StringOp.hh"
#include "checked_cast.hh"
#include "memory.hh"
#include "outer.hh"
#include "stl.hh"
#include "unreachable.hh"
#include <iomanip>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <iterator>

using std::string;
using std::vector;
using std::min;
using std::shared_ptr;

namespace openmsx {

// Global variables
bool MSXCPUInterface::breaked = false;
bool MSXCPUInterface::continued = false;
bool MSXCPUInterface::step = false;
MSXCPUInterface::BreakPoints MSXCPUInterface::breakPoints;
//TODO watchpoints
MSXCPUInterface::Conditions  MSXCPUInterface::conditions;

static std::unique_ptr<ReadOnlySetting> breakedSetting;
static unsigned breakedSettingCount = 0;


// Bitfields used in the disallowReadCache and disallowWriteCache arrays
static const byte SECUNDARY_SLOT_BIT = 0x01;
static const byte MEMORY_WATCH_BIT   = 0x02;
static const byte GLOBAL_WRITE_BIT   = 0x04;


MSXCPUInterface::MSXCPUInterface(MSXMotherBoard& motherBoard_)
	: memoryDebug       (motherBoard_)
	, slottedMemoryDebug(motherBoard_)
	, ioDebug           (motherBoard_)
	, slotInfo(motherBoard_.getMachineInfoCommand())
	, subSlottedInfo(motherBoard_.getMachineInfoCommand())
	, externalSlotInfo(motherBoard_.getMachineInfoCommand())
	, inputPortInfo (motherBoard_.getMachineInfoCommand())
	, outputPortInfo(motherBoard_.getMachineInfoCommand())
	, dummyDevice(DeviceFactory::createDummyDevice(
		*motherBoard_.getMachineConfig()))
	, msxcpu(motherBoard_.getCPU())
	, cliComm(motherBoard_.getMSXCliComm())
	, motherBoard(motherBoard_)
	, fastForward(false)
{
	for (int port = 0; port < 256; ++port) {
		IO_In [port] = dummyDevice.get();
		IO_Out[port] = dummyDevice.get();
	}
	for (int primSlot = 0; primSlot < 4; ++primSlot) {
		primarySlotState[primSlot] = 0;
		secondarySlotState[primSlot] = 0;
		expanded[primSlot] = 0;
		subSlotRegister[primSlot] = 0;
		for (int secSlot = 0; secSlot < 4; ++secSlot) {
			for (int page = 0; page < 4; ++page) {
				slotLayout[primSlot][secSlot][page] = dummyDevice.get();
			}
		}
	}
	for (auto& dev : visibleDevices) {
		dev = dummyDevice.get();
	}

	// initially allow all regions to be cached
	memset(disallowReadCache,  0, sizeof(disallowReadCache));
	memset(disallowWriteCache, 0, sizeof(disallowWriteCache));

	// Note: SlotState is initialised at reset

	msxcpu.setInterface(this);

	if (motherBoard.isTurboR()) {
		// TODO also MSX2+ needs (slightly different) VDPIODelay
		delayDevice = DeviceFactory::createVDPIODelay(
			*motherBoard.getMachineConfig(), *this);
		for (int port = 0x98; port <= 0x9B; ++port) {
			assert(IO_In [port] == dummyDevice.get());
			assert(IO_Out[port] == dummyDevice.get());
			IO_In [port] = delayDevice.get();
			IO_Out[port] = delayDevice.get();
		}
	}

	if (breakedSettingCount++ == 0) {
		assert(!breakedSetting);
		breakedSetting = make_unique<ReadOnlySetting>(
			motherBoard.getReactor().getCommandController(),
			"breaked", "Similar to 'debug breaked'",
			TclObject("false"));
	}
}

MSXCPUInterface::~MSXCPUInterface()
{
	if (--breakedSettingCount == 0) {
		assert(breakedSetting);
		breakedSetting = nullptr;
	}

	removeAllWatchPoints();

	if (delayDevice) {
		for (int port = 0x98; port <= 0x9B; ++port) {
			assert(IO_In [port] == delayDevice.get());
			assert(IO_Out[port] == delayDevice.get());
			IO_In [port] = dummyDevice.get();
			IO_Out[port] = dummyDevice.get();
		}
	}

	msxcpu.setInterface(nullptr);

	#ifndef NDEBUG
	for (int port = 0; port < 256; ++port) {
		if (IO_In[port] != dummyDevice.get()) {
			std::cout << "In-port " << port << " still registered "
			          << IO_In[port]->getName() << std::endl;
			UNREACHABLE;
		}
		if (IO_Out[port] != dummyDevice.get()) {
			std::cout << "Out-port " << port << " still registered "
			          << IO_Out[port]->getName() << std::endl;
			UNREACHABLE;
		}
	}
	for (int primSlot = 0; primSlot < 4; ++primSlot) {
		assert(!isExpanded(primSlot));
		for (int secSlot = 0; secSlot < 4; ++secSlot) {
			for (int page = 0; page < 4; ++page) {
				assert(slotLayout[primSlot][secSlot][page] == dummyDevice.get());
			}
		}
	}
	#endif
}

void MSXCPUInterface::removeAllWatchPoints()
{
	while (!watchPoints.empty()) {
		removeWatchPoint(watchPoints.back());
	}

}

byte MSXCPUInterface::readMemSlow(word address, EmuTime::param time)
{
	// something special in this region?
	if (unlikely(disallowReadCache[address >> CacheLine::BITS])) {
		// execute read watches before actual read
		if (readWatchSet[address >> CacheLine::BITS]
		                [address &  CacheLine::LOW]) {
			executeMemWatch(WatchPoint::READ_MEM, address);
		}
	}
	if (unlikely((address == 0xFFFF) && isExpanded(primarySlotState[3]))) {
		return 0xFF ^ subSlotRegister[primarySlotState[3]];
	} else {
		return visibleDevices[address >> 14]->readMem(address, time);
	}
}

void MSXCPUInterface::writeMemSlow(word address, byte value, EmuTime::param time)
{
	if (unlikely((address == 0xFFFF) && isExpanded(primarySlotState[3]))) {
		setSubSlot(primarySlotState[3], value);
		// Confirmed on turboR GT machine: write does _not_ also go to
		// the underlying (hidden) device. But it's theoretically
		// possible other slotexpanders behave different.
	} else {
		visibleDevices[address>>14]->writeMem(address, value, time);
	}
	// something special in this region?
	if (unlikely(disallowWriteCache[address >> CacheLine::BITS])) {
		// slot-select-ignore writes (Super Lode Runner)
		for (auto& g : globalWrites) {
			// very primitive address selection mechanism,
			// but more than enough for now
			if (unlikely(g.addr == address)) {
				g.device->globalWrite(address, value, time);
			}
		}
		// execute write watches after actual write
		if (writeWatchSet[address >> CacheLine::BITS]
		                 [address &  CacheLine::LOW]) {
			executeMemWatch(WatchPoint::WRITE_MEM, address, value);
		}
	}
}

void MSXCPUInterface::setExpanded(int ps)
{
	if (expanded[ps] == 0) {
		for (int page = 0; page < 4; ++page) {
			if (slotLayout[ps][0][page] != dummyDevice.get()) {
				throw MSXException("Can't expand slot because "
				                   "it's already in use.");
			}
		}
	}
	expanded[ps]++;
	changeExpanded(isExpanded(primarySlotState[3]));
}

void MSXCPUInterface::testUnsetExpanded(
		int ps, vector<MSXDevice*> allowed) const
{
	// TODO handle multi-devices
	allowed.push_back(dummyDevice.get());
	sort(begin(allowed), end(allowed)); // for set_difference()
	assert(isExpanded(ps));
	if (expanded[ps] != 1) return; // ok, still expanded after this

	std::vector<MSXDevice*> inUse;
	for (int ss = 0; ss < 4; ++ss) {
		for (int page = 0; page < 4; ++page) {
			MSXDevice* device = slotLayout[ps][ss][page];
			std::vector<MSXDevice*> devices;
			if (auto memDev = dynamic_cast<MSXMultiMemDevice*>(device)) {
				devices = memDev->getDevices();
				sort(begin(devices), end(devices)); // for set_difference()
			} else {
				devices.push_back(device);
			}
			std::set_difference(begin(devices), end(devices),
			                    begin(allowed), end(allowed),
			                    std::inserter(inUse, end(inUse)));

		}
	}
	if (inUse.empty()) return; // ok, no more devices in use

	StringOp::Builder msg;
	msg << "Can't remove slot expander from slot " << ps
	    << " because the following devices are still inserted:";
	for (auto& d : inUse) {
		msg << " " << d->getName();
	}
	msg << '.';
	throw MSXException(msg);
}

void MSXCPUInterface::unsetExpanded(int ps)
{
#ifndef NDEBUG
	try {
		vector<MSXDevice*> dummy;
		testUnsetExpanded(ps, dummy);
	} catch (...) {
		UNREACHABLE;
	}
#endif
	expanded[ps]--;
	changeExpanded(isExpanded(primarySlotState[3]));
}

void MSXCPUInterface::changeExpanded(bool newExpanded)
{
	if (newExpanded) {
		disallowReadCache [0xFF] |=  SECUNDARY_SLOT_BIT;
		disallowWriteCache[0xFF] |=  SECUNDARY_SLOT_BIT;
	} else {
		disallowReadCache [0xFF] &= ~SECUNDARY_SLOT_BIT;
		disallowWriteCache[0xFF] &= ~SECUNDARY_SLOT_BIT;
	}
	msxcpu.invalidateMemCache(0xFFFF & CacheLine::HIGH, 0x100);
}

MSXDevice*& MSXCPUInterface::getDevicePtr(byte port, bool isIn)
{
	MSXDevice** devicePtr = isIn ? &IO_In[port] : &IO_Out[port];
	while (auto watch = dynamic_cast<MSXWatchIODevice*>(*devicePtr)) {
		devicePtr = &watch->getDevicePtr();
	}
	if (*devicePtr == delayDevice.get()) {
		devicePtr = isIn ? &delayDevice->getInDevicePtr (port)
		                 : &delayDevice->getOutDevicePtr(port);
	}
	return *devicePtr;
}

void MSXCPUInterface::register_IO_In(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = getDevicePtr(port, true); // in
	register_IO(port, true, devicePtr, device); // in
}

void MSXCPUInterface::unregister_IO_In(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = getDevicePtr(port, true); // in
	unregister_IO(devicePtr, device);
}

void MSXCPUInterface::register_IO_Out(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = getDevicePtr(port, false); // out
	register_IO(port, false, devicePtr, device); // out
}

void MSXCPUInterface::unregister_IO_Out(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = getDevicePtr(port, false); // out
	unregister_IO(devicePtr, device);
}

void MSXCPUInterface::register_IO(int port, bool isIn,
                                  MSXDevice*& devicePtr, MSXDevice* device)
{
	if (devicePtr == dummyDevice.get()) {
		// first, replace DummyDevice
		devicePtr = device;
	} else {
		if (auto multi = dynamic_cast<MSXMultiIODevice*>(devicePtr)) {
			// third or more, add to existing MultiIO device
			multi->addDevice(device);
		} else {
			// second, create a MultiIO device
			multi = new MSXMultiIODevice(device->getHardwareConfig());
			multi->addDevice(devicePtr);
			multi->addDevice(device);
			devicePtr = multi;
		}
		if (isIn) {
			cliComm.printWarning(
				"Conflicting input port 0x" +
				StringOp::toHexString(port, 2) +
				" for devices " + devicePtr->getName());
		}
	}
}

void MSXCPUInterface::unregister_IO(MSXDevice*& devicePtr, MSXDevice* device)
{
	if (auto multi = dynamic_cast<MSXMultiIODevice*>(devicePtr)) {
		// remove from MultiIO device
		multi->removeDevice(device);
		auto& devices = multi->getDevices();
		if (devices.size() == 1) {
			// only one remaining, remove MultiIO device
			devicePtr = devices.front();
			devices.pop_back();
			delete multi;
		}
	} else {
		// remove last, put back DummyDevice
		assert(devicePtr == device);
		devicePtr = dummyDevice.get();
	}
}

MSXDevice* MSXCPUInterface::wrap_IO_In(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = getDevicePtr(port, true); // in
	MSXDevice* result = devicePtr;
	devicePtr = device;
	return result;
}
MSXDevice* MSXCPUInterface::wrap_IO_Out(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = getDevicePtr(port, false); // out
	MSXDevice* result = devicePtr;
	devicePtr = device;
	return result;
}
void MSXCPUInterface::unwrap_IO_In(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = getDevicePtr(port, true); // in
	devicePtr = device;
}
void MSXCPUInterface::unwrap_IO_Out(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = getDevicePtr(port, false); // out
	devicePtr = device;
}

static void reportMemOverlap(int ps, int ss, MSXDevice& dev1, MSXDevice& dev2)
{
	throw MSXException(StringOp::Builder() <<
		"Overlapping memory devices in slot " << ps << '.' << ss <<
		": " << dev1.getName() << " and " << dev2.getName() << '.');
}

void MSXCPUInterface::testRegisterSlot(
	MSXDevice& device, int ps, int ss, int base, int size)
{
	int page = base >> 14;
	MSXDevice*& slot = slotLayout[ps][ss][page];
	if (size == 0x4000) {
		// full 16kb, directly register device (no multiplexer)
		if (slot != dummyDevice.get()) {
			reportMemOverlap(ps, ss, *slot, device);
		}
	} else {
		// partial page
		if (slot == dummyDevice.get()) {
			// first, ok
		} else if (auto multi = dynamic_cast<MSXMultiMemDevice*>(slot)) {
			// second (or more), check for overlap
			if (!multi->canAdd(base, size)) {
				reportMemOverlap(ps, ss, *slot, device);
			}
		} else {
			// conflict with 'full ranged' device
			reportMemOverlap(ps, ss, *slot, device);
		}
	}
}

void MSXCPUInterface::registerSlot(
	MSXDevice& device, int ps, int ss, int base, int size)
{
	int page = base >> 14;
	MSXDevice*& slot = slotLayout[ps][ss][page];
	if (size == 0x4000) {
		// full 16kb, directly register device (no multiplexer)
		assert(slot == dummyDevice.get());
		slot = &device;
	} else {
		// partial page
		if (slot == dummyDevice.get()) {
			// first
			auto* multi = new MSXMultiMemDevice(device.getHardwareConfig());
			multi->add(device, base, size);
			slot = multi;
		} else if (auto* multi = dynamic_cast<MSXMultiMemDevice*>(slot)) {
			// second or more
			assert(multi->canAdd(base, size));
			multi->add(device, base, size);
		} else {
			// conflict with 'full ranged' device
			assert(false);
		}
	}
	updateVisible(page);
}

void MSXCPUInterface::unregisterSlot(
	MSXDevice& device, int ps, int ss, int base, int size)
{
	int page = base >> 14;
	MSXDevice*& slot = slotLayout[ps][ss][page];
	if (auto* multi = dynamic_cast<MSXMultiMemDevice*>(slot)) {
		// partial range
		multi->remove(device, base, size);
		if (multi->empty()) {
			delete multi;
			slot = dummyDevice.get();
		}
	} else {
		// full 16kb range
		assert(slot == &device);
		slot = dummyDevice.get();
	}
	updateVisible(page);
}

void MSXCPUInterface::registerMemDevice(
	MSXDevice& device, int ps, int ss, int base_, int size_)
{
	if (!isExpanded(ps) && (ss != 0)) {
		throw MSXException(StringOp::Builder() <<
			"Slot " << ps << '.' << ss <<
			" does not exist because slot is not expanded.");
	}

	// split range on 16kb borders
	// first check if registration is possible
	int base = base_;
	int size = size_;
	while (size > 0) {
		int partialSize = min(size, ((base + 0x4000) & ~0x3FFF) - base);
		testRegisterSlot(device, ps, ss, base, partialSize);
		base += partialSize;
		size -= partialSize;
	}
	// if all checks are successful, only then actually register
	base = base_;
	size = size_;
	while (size > 0) {
		int partialSize = min(size, ((base + 0x4000) & ~0x3FFF) - base);
		registerSlot(device, ps, ss, base, partialSize);
		base += partialSize;
		size -= partialSize;
	}
}

void MSXCPUInterface::unregisterMemDevice(
	MSXDevice& device, int ps, int ss, int base, int size)
{
	// split range on 16kb borders
	while (size > 0) {
		int partialSize = min(size, ((base + 0x4000) & ~0x3FFF) - base);
		unregisterSlot(device, ps, ss, base, partialSize);
		base += partialSize;
		size -= partialSize;
	}
}

void MSXCPUInterface::registerGlobalWrite(MSXDevice& device, word address)
{
	globalWrites.push_back({&device, address});

	disallowWriteCache[address >> CacheLine::BITS] |= GLOBAL_WRITE_BIT;
	msxcpu.invalidateMemCache(address & CacheLine::HIGH, 0x100);
}

void MSXCPUInterface::unregisterGlobalWrite(MSXDevice& device, word address)
{
	GlobalWriteInfo info = { &device, address };
	move_pop_back(globalWrites, rfind_unguarded(globalWrites, info));

	for (auto& g : globalWrites) {
		if ((g.addr >> CacheLine::BITS) ==
		    (address  >> CacheLine::BITS)) {
			// there is still a global write in this region
			return;
		}
	}
	disallowWriteCache[address >> CacheLine::BITS] &= ~GLOBAL_WRITE_BIT;
	msxcpu.invalidateMemCache(address & CacheLine::HIGH, 0x100);
}

ALWAYS_INLINE void MSXCPUInterface::updateVisible(int page, int ps, int ss)
{
	MSXDevice* newDevice = slotLayout[ps][ss][page];
	if (visibleDevices[page] != newDevice) {
		visibleDevices[page] = newDevice;
		msxcpu.updateVisiblePage(page, ps, ss);
	}
}
void MSXCPUInterface::updateVisible(int page)
{
	updateVisible(page, primarySlotState[page], secondarySlotState[page]);
}

void MSXCPUInterface::reset()
{
	for (auto& reg : subSlotRegister) {
		reg = 0;
	}
	setPrimarySlots(0);
}

byte MSXCPUInterface::readIRQVector()
{
	return motherBoard.readIRQVector();
}

void MSXCPUInterface::setPrimarySlots(byte value)
{
	// Change the slot structure.
	// Originally the code below was a loop over the 4 pages, and the check
	// for (un)expanded-slot was done unconditionally at the end. I've
	// completely unrolled the loop and only check for (un)expanded slot
	// when the slot in page 3 has changed. I've also added checks for slot
	// changes for the other 3 pages. Usually when this register is written
	// only one of the 4 pages actually changes, so these extra checks do
	// pay off. This does make the code a bit more complex (and the
	// generated code slightly bigger), but it does make a measurable speed
	// difference.  Changing the slots several hundreds of times per
	// (EmuTime) is not unusual. So this routine ended up quite high
	// (top-10) in some profile results.
	int ps0 = (value >> 0) & 3;
	if (unlikely(primarySlotState[0] != ps0)) {
		primarySlotState[0] = ps0;
		int ss0 = (subSlotRegister[ps0] >> 0) & 3;
		secondarySlotState[0] = ss0;
		updateVisible(0, ps0, ss0);
	}
	int ps1 = (value >> 2) & 3;
	if (unlikely(primarySlotState[1] != ps1)) {
		primarySlotState[1] = ps1;
		int ss1 = (subSlotRegister[ps1] >> 2) & 3;
		secondarySlotState[1] = ss1;
		updateVisible(1, ps1, ss1);
	}
	int ps2 = (value >> 4) & 3;
	if (unlikely(primarySlotState[2] != ps2)) {
		primarySlotState[2] = ps2;
		int ss2 = (subSlotRegister[ps2] >> 4) & 3;
		secondarySlotState[2] = ss2;
		updateVisible(2, ps2, ss2);
	}
	int ps3 = (value >> 6) & 3;
	if (unlikely(primarySlotState[3] != ps3)) {
		bool oldExpanded = isExpanded(primarySlotState[3]);
		bool newExpanded = isExpanded(ps3);
		primarySlotState[3] = ps3;
		int ss3 = (subSlotRegister[ps3] >> 6) & 3;
		secondarySlotState[3] = ss3;
		updateVisible(3, ps3, ss3);
		if (unlikely(oldExpanded != newExpanded)) {
			changeExpanded(newExpanded);
		}
	}
}

void MSXCPUInterface::setSubSlot(byte primSlot, byte value)
{
	subSlotRegister[primSlot] = value;
	for (int page = 0; page < 4; ++page, value >>= 2) {
		if (primSlot == primarySlotState[page]) {
			secondarySlotState[page] = value & 3;
			// Change the visible devices
			updateVisible(page);
		}
	}
}

byte MSXCPUInterface::peekMem(word address, EmuTime::param time) const
{
	if ((address == 0xFFFF) && isExpanded(primarySlotState[3])) {
		return 0xFF ^ subSlotRegister[primarySlotState[3]];
	} else {
		return visibleDevices[address >> 14]->peekMem(address, time);
	}
}

byte MSXCPUInterface::peekSlottedMem(unsigned address, EmuTime::param time) const
{
	byte primSlot = (address & 0xC0000) >> 18;
	byte subSlot = (address & 0x30000) >> 16;
	byte page = (address & 0x0C000) >> 14;
	word offset = (address & 0xFFFF); // includes page
	if (!isExpanded(primSlot)) {
		subSlot = 0;
	}

	if ((offset == 0xFFFF) && isExpanded(primSlot)) {
		return 0xFF ^ subSlotRegister[primSlot];
	} else {
		return slotLayout[primSlot][subSlot][page]->peekMem(offset, time);
	}
}

byte MSXCPUInterface::readSlottedMem(unsigned address, EmuTime::param time)
{
	byte primSlot = (address & 0xC0000) >> 18;
	byte subSlot = (address & 0x30000) >> 16;
	byte page = (address & 0x0C000) >> 14;
	word offset = (address & 0xFFFF); // includes page
	if (!isExpanded(primSlot)) {
		subSlot = 0;
	}

	if ((offset == 0xFFFF) && isExpanded(primSlot)) {
		return 0xFF ^ subSlotRegister[primSlot];
	} else {
		return slotLayout[primSlot][subSlot][page]->peekMem(offset, time);
	}
}

void MSXCPUInterface::writeSlottedMem(unsigned address, byte value,
                                      EmuTime::param time)
{
	byte primSlot = (address & 0xC0000) >> 18;
	byte subSlot = (address & 0x30000) >> 16;
	byte page = (address & 0x0C000) >> 14;
	word offset = (address & 0xFFFF); // includes page
	if (!isExpanded(primSlot)) {
		subSlot = 0;
	}

	if ((offset == 0xFFFF) && isExpanded(primSlot)) {
		setSubSlot(primSlot, value);
	} else {
		slotLayout[primSlot][subSlot][page]->writeMem(offset, value, time);
	}
}

void MSXCPUInterface::insertBreakPoint(const BreakPoint& bp)
{
	auto it = upper_bound(begin(breakPoints), end(breakPoints),
	                      bp, CompareBreakpoints());
	breakPoints.insert(it, bp);
}

void MSXCPUInterface::removeBreakPoint(const BreakPoint& bp)
{
	auto range = equal_range(begin(breakPoints), end(breakPoints),
	                         bp.getAddress(), CompareBreakpoints());
	breakPoints.erase(find_if_unguarded(range.first, range.second,
		[&](const BreakPoint& i) { return &i == &bp; }));
}

void MSXCPUInterface::checkBreakPoints(
	std::pair<BreakPoints::const_iterator,
	          BreakPoints::const_iterator> range,
	MSXMotherBoard& motherBoard)
{
	// create copy for the case that breakpoint/condition removes itself
	//  - keeps object alive by holding a shared_ptr to it
	//  - avoids iterating over a changing collection
	BreakPoints bpCopy(range.first, range.second);
	auto& globalCliComm = motherBoard.getReactor().getGlobalCliComm();
	auto& interp        = motherBoard.getReactor().getInterpreter();
	for (auto& p : bpCopy) {
		p.checkAndExecute(globalCliComm, interp);
	}
	auto condCopy = conditions;
	for (auto& c : condCopy) {
		c.checkAndExecute(globalCliComm, interp);
	}
}


void MSXCPUInterface::setWatchPoint(const shared_ptr<WatchPoint>& watchPoint)
{
	watchPoints.push_back(watchPoint);
	WatchPoint::Type type = watchPoint->getType();
	switch (type) {
	case WatchPoint::READ_IO:
		registerIOWatch(*watchPoint, IO_In);
		break;
	case WatchPoint::WRITE_IO:
		registerIOWatch(*watchPoint, IO_Out);
		break;
	case WatchPoint::READ_MEM:
	case WatchPoint::WRITE_MEM:
		updateMemWatch(type);
		break;
	default:
		UNREACHABLE; break;
	}
}

void MSXCPUInterface::removeWatchPoint(shared_ptr<WatchPoint> watchPoint)
{
	// Pass shared_ptr by value to keep the object alive for the duration
	// of this function, otherwise it gets deleted as soon as it's removed
	// from the watchPoints collection.
	for (auto it = begin(watchPoints); it != end(watchPoints); ++it) {
		if (*it == watchPoint) {
			// remove before calling updateMemWatch()
			watchPoints.erase(it);
			WatchPoint::Type type = watchPoint->getType();
			switch (type) {
			case WatchPoint::READ_IO:
				unregisterIOWatch(*watchPoint, IO_In);
				break;
			case WatchPoint::WRITE_IO:
				unregisterIOWatch(*watchPoint, IO_Out);
				break;
			case WatchPoint::READ_MEM:
			case WatchPoint::WRITE_MEM:
				updateMemWatch(type);
				break;
			default:
				UNREACHABLE; break;
			}
			break;
		}
	}
}

void MSXCPUInterface::setCondition(const DebugCondition& cond)
{
	conditions.push_back(cond);
}

void MSXCPUInterface::removeCondition(const DebugCondition& cond)
{
	conditions.erase(rfind_if_unguarded(conditions,
		[&](DebugCondition& e) { return &e == &cond; }));
}

void MSXCPUInterface::registerIOWatch(WatchPoint& watchPoint, MSXDevice** devices)
{
	assert(dynamic_cast<WatchIO*>(&watchPoint));
	auto& ioWatch = static_cast<WatchIO&>(watchPoint);
	unsigned beginPort = ioWatch.getBeginAddress();
	unsigned endPort   = ioWatch.getEndAddress();
	assert(beginPort <= endPort);
	assert(endPort < 0x100);
	for (unsigned port = beginPort; port <= endPort; ++port) {
		ioWatch.getDevice(port).getDevicePtr() = devices[port];
		devices[port] = &ioWatch.getDevice(port);
	}
}

void MSXCPUInterface::unregisterIOWatch(WatchPoint& watchPoint, MSXDevice** devices)
{
	assert(dynamic_cast<WatchIO*>(&watchPoint));
	auto& ioWatch = static_cast<WatchIO&>(watchPoint);
	unsigned beginPort = ioWatch.getBeginAddress();
	unsigned endPort   = ioWatch.getEndAddress();
	assert(beginPort <= endPort);
	assert(endPort < 0x100);

	for (unsigned port = beginPort; port <= endPort; ++port) {
		// find pointer to watchpoint
		MSXDevice** prev = &devices[port];
		while (*prev != &ioWatch.getDevice(port)) {
			prev = &checked_cast<MSXWatchIODevice*>(*prev)->getDevicePtr();
		}
		// remove watchpoint from chain
		*prev = checked_cast<MSXWatchIODevice*>(*prev)->getDevicePtr();
	}
}

void MSXCPUInterface::updateMemWatch(WatchPoint::Type type)
{
	std::bitset<CacheLine::SIZE>* watchSet =
		(type == WatchPoint::READ_MEM) ? readWatchSet : writeWatchSet;
	for (unsigned i = 0; i < CacheLine::NUM; ++i) {
		watchSet[i].reset();
	}
	for (auto& w : watchPoints) {
		if (w->getType() == type) {
			unsigned beginAddr = w->getBeginAddress();
			unsigned endAddr   = w->getEndAddress();
			assert(beginAddr <= endAddr);
			assert(endAddr < 0x10000);
			for (unsigned addr = beginAddr; addr <= endAddr; ++addr) {
				watchSet[addr >> CacheLine::BITS].set(
				         addr  & CacheLine::LOW);
			}
		}
	}
	for (unsigned i = 0; i < CacheLine::NUM; ++i) {
		if (readWatchSet [i].any()) {
			disallowReadCache [i] |=  MEMORY_WATCH_BIT;
		} else {
			disallowReadCache [i] &= ~MEMORY_WATCH_BIT;
		}
		if (writeWatchSet[i].any()) {
			disallowWriteCache[i] |=  MEMORY_WATCH_BIT;
		} else {
			disallowWriteCache[i] &= ~MEMORY_WATCH_BIT;
		}
	}
	msxcpu.invalidateMemCache(0x0000, 0x10000);
}

void MSXCPUInterface::executeMemWatch(WatchPoint::Type type,
                                      unsigned address, unsigned value)
{
	assert(!watchPoints.empty());
	if (isFastForward()) return;

	auto& globalCliComm = motherBoard.getReactor().getGlobalCliComm();
	auto& interp        = motherBoard.getReactor().getInterpreter();
	interp.setVariable(TclObject("wp_last_address"),
	                   TclObject(int(address)));
	if (value != ~0u) {
		interp.setVariable(TclObject("wp_last_value"),
		                   TclObject(int(value)));
	}

	auto wpCopy = watchPoints;
	for (auto& w : wpCopy) {
		if ((w->getBeginAddress() <= address) &&
		    (w->getEndAddress()   >= address) &&
		    (w->getType()         == type)) {
			w->checkAndExecute(globalCliComm, interp);
		}
	}

	interp.unsetVariable("wp_last_address");
	interp.unsetVariable("wp_last_value");
}


void MSXCPUInterface::doBreak()
{
	assert(!isFastForward());
	if (breaked) return;
	breaked = true;
	msxcpu.exitCPULoopSync();

	Reactor& reactor = motherBoard.getReactor();
	reactor.block();
	breakedSetting->setReadOnlyValue(TclObject("true"));
	reactor.getCliComm().update(CliComm::STATUS, "cpu", "suspended");
	reactor.getEventDistributor().distributeEvent(
		std::make_shared<SimpleEvent>(OPENMSX_BREAK_EVENT));
}

void MSXCPUInterface::doStep()
{
	assert(!isFastForward());
	if (breaked) {
		step = true;
		doContinue2();
	}
}

void MSXCPUInterface::doContinue()
{
	assert(!isFastForward());
	if (breaked) {
		continued = true;
		doContinue2();
	}
}

void MSXCPUInterface::doContinue2()
{
	breaked = false;
	Reactor& reactor = motherBoard.getReactor();
	breakedSetting->setReadOnlyValue(TclObject("false"));
	reactor.getCliComm().update(CliComm::STATUS, "cpu", "running");
	reactor.unblock();
}

void MSXCPUInterface::cleanup()
{
	// before the Tcl interpreter is destroyed, we must delete all
	// TclObjects. Breakpoints and conditions contain such objects
	// for the condition and action.
	// TODO it would be nicer if breakpoints and conditions were not
	//      global objects.
	breakPoints.clear();
	conditions.clear();
}


// class MemoryDebug

MSXCPUInterface::MemoryDebug::MemoryDebug(MSXMotherBoard& motherBoard_)
	: SimpleDebuggable(motherBoard_, "memory",
	                   "The memory currently visible for the CPU.", 0x10000)
{
}

byte MSXCPUInterface::MemoryDebug::read(unsigned address, EmuTime::param time)
{
	auto& interface = OUTER(MSXCPUInterface, memoryDebug);
	return interface.peekMem(address, time);
}

void MSXCPUInterface::MemoryDebug::write(unsigned address, byte value,
                                         EmuTime::param time)
{
	auto& interface = OUTER(MSXCPUInterface, memoryDebug);
	return interface.writeMem(address, value, time);
}


// class SlottedMemoryDebug

MSXCPUInterface::SlottedMemoryDebug::SlottedMemoryDebug(
		MSXMotherBoard& motherBoard_)
	: SimpleDebuggable(motherBoard_, "slotted memory",
	                   "The memory in slots and subslots.", 0x10000 * 4 * 4)
{
}

byte MSXCPUInterface::SlottedMemoryDebug::read(unsigned address, EmuTime::param time)
{
	auto& interface = OUTER(MSXCPUInterface, slottedMemoryDebug);
	return interface.peekSlottedMem(address, time);
}

void MSXCPUInterface::SlottedMemoryDebug::write(unsigned address, byte value,
                                                EmuTime::param time)
{
	auto& interface = OUTER(MSXCPUInterface, slottedMemoryDebug);
	return interface.writeSlottedMem(address, value, time);
}


// class SlotInfo

static unsigned getSlot(
	Interpreter& interp, const TclObject& token, const string& itemName)
{
	unsigned slot = token.getInt(interp);
	if (slot >= 4) {
		throw CommandException(itemName + " must be in range 0..3");
	}
	return slot;
}

MSXCPUInterface::SlotInfo::SlotInfo(
		InfoCommand& machineInfoCommand)
	: InfoTopic(machineInfoCommand, "slot")
{
}

void MSXCPUInterface::SlotInfo::execute(array_ref<TclObject> tokens,
                       TclObject& result) const
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	auto& interp = getInterpreter();
	unsigned ps   = getSlot(interp, tokens[2], "Primary slot");
	unsigned ss   = getSlot(interp, tokens[3], "Secondary slot");
	unsigned page = getSlot(interp, tokens[4], "Page");
	auto& interface = OUTER(MSXCPUInterface, slotInfo);
	if (!interface.isExpanded(ps)) {
		ss = 0;
	}
	interface.slotLayout[ps][ss][page]->getNameList(result);
}

string MSXCPUInterface::SlotInfo::help(const vector<string>& /*tokens*/) const
{
	return "Retrieve name of the device inserted in given "
	       "primary slot / secondary slot / page.";
}


// class SubSlottedInfo

MSXCPUInterface::SubSlottedInfo::SubSlottedInfo(
		InfoCommand& machineInfoCommand)
	: InfoTopic(machineInfoCommand, "issubslotted")
{
}

void MSXCPUInterface::SubSlottedInfo::execute(array_ref<TclObject> tokens,
                             TclObject& result) const
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	auto& interface = OUTER(MSXCPUInterface, subSlottedInfo);
	result.setInt(interface.isExpanded(
		getSlot(getInterpreter(), tokens[2], "Slot")));
}

string MSXCPUInterface::SubSlottedInfo::help(
	const vector<string>& /*tokens*/) const
{
	return "Indicates whether a certain primary slot is expanded.";
}


// class ExternalSlotInfo

MSXCPUInterface::ExternalSlotInfo::ExternalSlotInfo(
		InfoCommand& machineInfoCommand)
	: InfoTopic(machineInfoCommand, "isexternalslot")
{
}

void MSXCPUInterface::ExternalSlotInfo::execute(
	array_ref<TclObject> tokens, TclObject& result) const
{
	int ps = 0;
	int ss = 0;
	auto& interp = getInterpreter();
	switch (tokens.size()) {
	case 4:
		ss = getSlot(interp, tokens[3], "Secondary slot");
		// Fall-through
	case 3:
		ps = getSlot(interp, tokens[2], "Primary slot");
		break;
	default:
		throw SyntaxError();
	}
	auto& interface = OUTER(MSXCPUInterface, externalSlotInfo);
	auto& manager = interface.motherBoard.getSlotManager();
	result.setInt(manager.isExternalSlot(ps, ss, true));
}

string MSXCPUInterface::ExternalSlotInfo::help(
	const vector<string>& /*tokens*/) const
{
	return "Indicates whether a certain slot is external or internal.";
}


// class IODebug

MSXCPUInterface::IODebug::IODebug(MSXMotherBoard& motherBoard_)
	: SimpleDebuggable(motherBoard_, "ioports", "IO ports.", 0x100)
{
}

byte MSXCPUInterface::IODebug::read(unsigned address, EmuTime::param time)
{
	auto& interface = OUTER(MSXCPUInterface, ioDebug);
	return interface.IO_In[address & 0xFF]->peekIO(address, time);
}

void MSXCPUInterface::IODebug::write(unsigned address, byte value, EmuTime::param time)
{
	auto& interface = OUTER(MSXCPUInterface, ioDebug);
	interface.writeIO(word(address), value, time);
}


// class IOInfo

MSXCPUInterface::IOInfo::IOInfo(InfoCommand& machineInfoCommand, const char* name_)
	: InfoTopic(machineInfoCommand, name_)
{
}

void MSXCPUInterface::IOInfo::helper(
	array_ref<TclObject> tokens, TclObject& result, MSXDevice** devices) const
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	unsigned port = tokens[2].getInt(getInterpreter());
	if (port >= 256) {
		throw CommandException("Port must be in range 0..255");
	}
	result.setString(devices[port]->getName());
}
void MSXCPUInterface::IInfo::execute(
	array_ref<TclObject> tokens, TclObject& result) const
{
	auto& interface = OUTER(MSXCPUInterface, inputPortInfo);
	helper(tokens, result, interface.IO_In);
}
void MSXCPUInterface::OInfo::execute(
	array_ref<TclObject> tokens, TclObject& result) const
{
	auto& interface = OUTER(MSXCPUInterface, outputPortInfo);
	helper(tokens, result, interface.IO_Out);
}

string MSXCPUInterface::IOInfo::help(const vector<string>& /*tokens*/) const
{
	return "Return the name of the device connected to the given IO port.";
}


template<typename Archive>
void MSXCPUInterface::serialize(Archive& ar, unsigned /*version*/)
{
	// TODO watchPoints ???

	// primary and 4 secundary slot select registers
	byte prim = 0;
	if (!ar.isLoader()) {
		for (int i = 0; i < 4; ++i) {
			prim |= primarySlotState[i] << (2 * i);
		}
	}
	ar.serialize("primarySlots", prim);
	ar.serialize("subSlotRegs", subSlotRegister);
	if (ar.isLoader()) {
		setPrimarySlots(prim);
		for (int i = 0; i < 4; ++i) {
			setSubSlot(i, subSlotRegister[i]);
		}
	}

	if (delayDevice) {
		ar.serialize("vdpDelay", *delayDevice);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXCPUInterface);

} // namespace openmsx
