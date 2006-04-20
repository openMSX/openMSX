// $Id$

#include "MSXCPUInterface.hh"
#include "DummyDevice.hh"
#include "SimpleDebuggable.hh"
#include "Command.hh"
#include "InfoTopic.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "XMLElement.hh"
#include "VDPIODelay.hh"
#include "CliComm.hh"
#include "MSXMultiIODevice.hh"
#include "MSXMultiMemDevice.hh"
#include "MSXWatchIODevice.hh"
#include "MSXException.hh"
#include "CartridgeSlotManager.hh"
#include "DeviceFactory.hh"
#include <cstdio>
#include <memory> // for auto_ptr
#include <set>
#include <sstream>
#include <iomanip>
#include <algorithm>

using std::auto_ptr;
using std::ostringstream;
using std::setfill;
using std::setw;
using std::uppercase;
using std::string;
using std::vector;
using std::min;

namespace openmsx {

class MemoryDebug : public SimpleDebuggable
{
public:
	MemoryDebug(MSXCPUInterface& interface,
	            MSXMotherBoard& motherBoard);
	virtual byte read(unsigned address, const EmuTime& time);
	virtual void write(unsigned address, byte value, const EmuTime& time);
private:
	MSXCPUInterface& interface;
};

class SlottedMemoryDebug : public SimpleDebuggable
{
public:
	SlottedMemoryDebug(MSXCPUInterface& interface,
	                   MSXMotherBoard& motherBoard);
	virtual byte read(unsigned address, const EmuTime& time);
	virtual void write(unsigned address, byte value, const EmuTime& time);
private:
	MSXCPUInterface& interface;
};

class IODebug : public SimpleDebuggable
{
public:
	IODebug(MSXCPUInterface& interface,
	        MSXMotherBoard& motherBoard);
	virtual byte read(unsigned address, const EmuTime& time);
	virtual void write(unsigned address, byte value, const EmuTime& time);
private:
	MSXCPUInterface& interface;
};

class SlotInfo : public InfoTopic
{
public:
	SlotInfo(CommandController& commandController,
	         MSXCPUInterface& interface);
	virtual void execute(const std::vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual std::string help(const std::vector<std::string>& tokens) const;
private:
	MSXCPUInterface& interface;
};

class SubSlottedInfo : public InfoTopic
{
public:
	SubSlottedInfo(CommandController& commandController,
		       MSXCPUInterface& interface);
	virtual void execute(const std::vector<TclObject*>& tokens,
			     TclObject& result) const;
	virtual std::string help(const std::vector<std::string>& tokens) const;
private:
	MSXCPUInterface& interface;
};

class ExternalSlotInfo : public InfoTopic
{
public:
	ExternalSlotInfo(CommandController& commandController,
			 CartridgeSlotManager& manager);
	virtual void execute(const std::vector<TclObject*>& tokens,
			     TclObject& result) const;
	virtual std::string help(const std::vector<std::string>& tokens) const;
private:
	CartridgeSlotManager& manager;
};

class IOInfo : public InfoTopic
{
public:
	IOInfo(CommandController& commandController,
	       MSXCPUInterface& interface, bool input);
	virtual void execute(const std::vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual std::string help(const std::vector<std::string>& tokens) const;
private:
	MSXCPUInterface& interface;
	bool input;
};


auto_ptr<MSXCPUInterface> MSXCPUInterface::create(
		MSXMotherBoard& motherBoard, const XMLElement& machineConfig)
{
	if (machineConfig.getChild("devices").findChild("S1990")) {
		return auto_ptr<MSXCPUInterface>(
			new TurborCPUInterface(motherBoard));
	} else {
		return auto_ptr<MSXCPUInterface>(
			new MSXCPUInterface(motherBoard));
	}
}

MSXCPUInterface::MSXCPUInterface(MSXMotherBoard& motherBoard)
	: memoryDebug       (new MemoryDebug       (*this, motherBoard))
	, slottedMemoryDebug(new SlottedMemoryDebug(*this, motherBoard))
	, ioDebug           (new IODebug           (*this, motherBoard))
	, slotInfo        (new SlotInfo(
		motherBoard.getCommandController(), *this))
	, subSlottedInfo  (new SubSlottedInfo(
		motherBoard.getCommandController(), *this))
	, externalSlotInfo(new ExternalSlotInfo(
		motherBoard.getCommandController(),
		motherBoard.getSlotManager()))
	, inputPortInfo (new IOInfo(motherBoard.getCommandController(), *this, true))
	, outputPortInfo(new IOInfo(motherBoard.getCommandController(), *this, false))
	, dummyDevice(motherBoard.getDummyDevice())
	, msxcpu(motherBoard.getCPU())
	, cliCommOutput(motherBoard.getCliComm())
{
	for (int port = 0; port < 256; ++port) {
		IO_In [port] = &dummyDevice;
		IO_Out[port] = &dummyDevice;
	}
	for (int primSlot = 0; primSlot < 4; ++primSlot) {
		primarySlotState[primSlot] = 0;
		secondarySlotState[primSlot] = 0;
		expanded[primSlot] = 0;
		subSlotRegister[primSlot] = 0;
		for (int secSlot = 0; secSlot < 4; ++secSlot) {
			for (int page = 0; page < 4; ++page) {
				slotLayout[primSlot][secSlot][page] = &dummyDevice;
			}
		}
	}
	for (int page = 0; page < 4; ++page) {
		visibleDevices[page] = &dummyDevice;
	}
	setAllowedCache();

	// Note: SlotState is initialised at reset

	msxcpu.setInterface(this);
}

MSXCPUInterface::~MSXCPUInterface()
{
	while (!watchPoints.empty()) {
		removeWatchPoint(*watchPoints.front());
	}
	
	msxcpu.setInterface(NULL);

	for (int port = 0; port < 256; ++port) {
		assert(IO_In [port] == &dummyDevice);
		assert(IO_Out[port] == &dummyDevice);
	}
	for (int primSlot = 0; primSlot < 4; ++primSlot) {
		assert(!isExpanded(primSlot));
		for (int secSlot = 0; secSlot < 4; ++secSlot) {
			for (int page = 0; page < 4; ++page) {
				assert(slotLayout[primSlot][secSlot][page] == &dummyDevice);
			}
		}
	}
}

byte MSXCPUInterface::readMemSlow(word address, const EmuTime& time)
{
	// execute read watches before actual read
	if (unlikely(readWatchSet[address >> CPU::CACHE_LINE_BITS]
	                         [address &  CPU::CACHE_LINE_LOW])) {
		executeMemWatch(address, WatchPoint::READ_MEM);
	}
	if (unlikely((address == 0xFFFF) && isExpanded(primarySlotState[3]))) {
		return 0xFF ^ subSlotRegister[primarySlotState[3]];
	} else {
		return visibleDevices[address >> 14]->readMem(address, time);
	}
}

void MSXCPUInterface::writeMemSlow(word address, byte value, const EmuTime& time)
{
	if (unlikely((address == 0xFFFF) && isExpanded(primarySlotState[3]))) {
		setSubSlot(primarySlotState[3], value);
	} else {
		visibleDevices[address>>14]->writeMem(address, value, time);
	}
	// execute write watches after actual write
	if (unlikely(writeWatchSet[address >> CPU::CACHE_LINE_BITS]
	                          [address &  CPU::CACHE_LINE_LOW])) {
		executeMemWatch(address, WatchPoint::WRITE_MEM);
	}
}

void MSXCPUInterface::setAllowedCache()
{
	memset(allowReadCache,  1, sizeof(allowReadCache));
	memset(allowWriteCache, 1, sizeof(allowWriteCache));
	if (isExpanded(primarySlotState[3])) {
		allowReadCache [255] = false;
		allowWriteCache[255] = false;
	}
	for (int i = 0; i < CPU::CACHE_LINE_NUM; ++i) {
		if (readWatchSet [i].any()) allowReadCache [i] = false;
		if (writeWatchSet[i].any()) allowWriteCache[i] = false;
	}
}

void MSXCPUInterface::setExpanded(int ps)
{
	if (expanded[ps] == 0) {
		for (int page = 0; page < 4; ++page) {
			if (slotLayout[ps][0][page] != &dummyDevice) {
				throw MSXException("Can't expand slot because "
				                   "it's already in use.");
			}
		}
	}
	expanded[ps]++;
}

void MSXCPUInterface::testUnsetExpanded(
		int ps, std::vector<MSXDevice*>& alreadyRemoved) const
{
	// TODO handle multi-devices
	std::set<MSXDevice*> allowed(alreadyRemoved.begin(), alreadyRemoved.end());
	allowed.insert(&dummyDevice);
	assert(isExpanded(ps));
	if (expanded[ps] == 1) {
		for (int ss = 0; ss < 4; ++ss) {
			for (int page = 0; page < 4; ++page) {
				if (allowed.find(slotLayout[ps][ss][page]) ==
				    allowed.end()) {
					throw MSXException(
						"Can't remove slotexpander "
						"because slot is still in use.");
				}
			}
		}
	}
}

void MSXCPUInterface::unsetExpanded(int ps)
{
#ifndef NDEBUG
	try {
		vector<MSXDevice*> dummy;
		testUnsetExpanded(ps, dummy);
	} catch (...) {
		assert(false);
	}
#endif
	expanded[ps]--;
}

MSXDevice*& MSXCPUInterface::getDevicePtr(byte port, bool isIn)
{
	MSXDevice** devicePtr = isIn ? &IO_In[port] : &IO_Out[port];
	while (MSXWatchIODevice* watch = dynamic_cast<MSXWatchIODevice*>(*devicePtr)) {
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
	PRT_DEBUG(device->getName() << " registers " << (isIn ? "In" : "Out") <<
	          "-port " << std::hex << port << std::dec);

	if (devicePtr == &dummyDevice) {
		// first, replace DummyDevice
		devicePtr = device;
	} else {
		if (MSXMultiIODevice* multi = dynamic_cast<MSXMultiIODevice*>(
		                                                   devicePtr)) {
			// third or more, add to existing MultiIO device
			multi->addDevice(device);
		} else {
			// second, create a MultiIO device
			multi = new MSXMultiIODevice(device->getMotherBoard());
			multi->addDevice(devicePtr);
			multi->addDevice(device);
			devicePtr = multi;
		}
		if (isIn) {
			cliCommOutput.printWarning(
				"Conflicting input port 0x" +
				StringOp::toHexString(port, 2) +
				" for devices " + devicePtr->getName());
		}
	}
}

void MSXCPUInterface::unregister_IO(MSXDevice*& devicePtr, MSXDevice* device)
{
	if (MSXMultiIODevice* multi = dynamic_cast<MSXMultiIODevice*>(
	                                                          devicePtr)) {
		// remove from MultiIO device
		multi->removeDevice(device);
		MSXMultiIODevice::Devices& devices = multi->getDevices();
		if (devices.size() == 1) {
			// only one remaining, remove MultiIO device
			devicePtr = devices.front();
			devices.pop_back();
			delete multi;
		}
	} else {
		// remove last, put back DummyDevice
		assert(devicePtr == device);
		devicePtr = &dummyDevice;
	}
}


static void reportMemOverlap(int ps, int ss, MSXDevice& dev1, MSXDevice& dev2)
{
	throw MSXException("Overlapping memory devices in slot " +
	                   StringOp::toString(ps) + "." +
	                   StringOp::toString(ss) + ": " +
	                   dev1.getName() + " and " + dev2.getName() + ".");
}

void MSXCPUInterface::registerSlot(
	MSXDevice& device, int ps, int ss, int base, int size)
{
	PRT_DEBUG(device.getName() << " registers at " <<
	          std::dec << ps << " " << ss << " 0x" <<
	          std::hex << base << "-0x" << (base + size - 1));
	if (!isExpanded(ps) && (ss != 0)) {
		throw MSXException(
			"Slot " + StringOp::toString(ps) +
			"."     + StringOp::toString(ss) +
			" does not exist because slot is not expanded.");
	}
	int page = base >> 14;
	MSXDevice*& slot = slotLayout[ps][ss][page];
	if (size == 0x4000) {
		// full 16kb, directly register device (no multiplexer)
		if (slot != &dummyDevice) {
			reportMemOverlap(ps, ss, *slot, device);
		}
		slot = &device;
	} else {
		// partial page
		if (slot == &dummyDevice) {
			// first
			MSXMultiMemDevice* multi = new MSXMultiMemDevice(
			                            device.getMotherBoard());
			multi->add(device, base, size);
			slot = multi;
		} else if (MSXMultiMemDevice* multi =
		                  dynamic_cast<MSXMultiMemDevice*>(slot)) {
			// second or more
			if (!multi->add(device, base, size)) {
				reportMemOverlap(ps, ss, *slot, device);
			}
		} else {
			// conflict with 'full ranged' device
			reportMemOverlap(ps, ss, *slot, device);
		}
	}
	updateVisible(page);
}

void MSXCPUInterface::unregisterSlot(
	MSXDevice& device, int ps, int ss, int base, int size)
{
	int page = base >> 14;
	MSXDevice*& slot = slotLayout[ps][ss][page];
	if (MSXMultiMemDevice* multi = dynamic_cast<MSXMultiMemDevice*>(slot)) {
		// partial range
		multi->remove(device, base, size);
		if (multi->empty()) {
			delete multi;
			slot = &dummyDevice;
		}
	} else {
		// full 16kb range
		assert(slot == &device);
		slot = &dummyDevice;
	}
	updateVisible(page);
}

void MSXCPUInterface::registerMemDevice(
	MSXDevice& device, int ps, int ss, int base, int size)
{
	// split range on 16kb borders
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

void MSXCPUInterface::updateVisible(int page)
{
	MSXDevice* newDevice = slotLayout[primarySlotState[page]]
	                                 [secondarySlotState[page]]
	                                 [page];
	if (visibleDevices[page] != newDevice) {
		visibleDevices[page] = newDevice;
		msxcpu.updateVisiblePage(page, primarySlotState[page],
		                               secondarySlotState[page]);
	}
	/*
	PRT_DEBUG(" page: " << (int)page <<
	          " ps: " << (int)primarySlotState[page] <<
	          " ss: " << (int)secondarySlotState[page] <<
	          " device: " << newDevice->getName());
	*/
}

void MSXCPUInterface::reset()
{
	for (int i = 0; i < 4; ++i) {
		subSlotRegister[i] = 0;
	}
	setPrimarySlots(0);
}

void MSXCPUInterface::setPrimarySlots(byte value)
{
	for (int page = 0; page < 4; ++page, value >>= 2) {
		// Change the slot structure
		primarySlotState[page] = value & 3;
		secondarySlotState[page] =
			(subSlotRegister[value & 3] >> (page * 2)) & 3;
		// Change the visible devices
		updateVisible(page);
	}
	setAllowedCache();
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

byte MSXCPUInterface::peekMem(word address, const EmuTime& time) const
{
	if ((address == 0xFFFF) && isExpanded(primarySlotState[3])) {
		return 0xFF ^ subSlotRegister[primarySlotState[3]];
	} else {
		return visibleDevices[address >> 14]->peekMem(address, time);
	}
}

byte MSXCPUInterface::peekSlottedMem(unsigned address, const EmuTime& time) const
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
                                      const EmuTime& time)
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


void MSXCPUInterface::setWatchPoint(std::auto_ptr<WatchPoint> watchPoint_)
{
	WatchPoint* watchPoint = watchPoint_.release();
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
		assert(false);
		break;
	}
}

void MSXCPUInterface::removeWatchPoint(WatchPoint& watchPoint)
{
	WatchPoints::iterator it = find(watchPoints.begin(), watchPoints.end(),
	                                &watchPoint);
	assert(it != watchPoints.end());
	watchPoints.erase(it);

	WatchPoint::Type type = watchPoint.getType();
	switch (type) {
	case WatchPoint::READ_IO:
		unregisterIOWatch(watchPoint, IO_In);
		break;
	case WatchPoint::WRITE_IO:
		unregisterIOWatch(watchPoint, IO_Out);
		break;
	case WatchPoint::READ_MEM:
	case WatchPoint::WRITE_MEM:
		updateMemWatch(type);
		break;
	default:
		assert(false);
		break;
	}
	delete &watchPoint;
}

const MSXCPUInterface::WatchPoints& MSXCPUInterface::getWatchPoints() const
{
	return watchPoints;
}

void MSXCPUInterface::registerIOWatch(WatchPoint& watchPoint, MSXDevice** devices)
{
	assert(dynamic_cast<MSXWatchIODevice*>(&watchPoint));
	MSXWatchIODevice& ioWatch = static_cast<MSXWatchIODevice&>(watchPoint);
	unsigned port = ioWatch.getAddress();
	assert(port < 256);
	ioWatch.getDevicePtr() = devices[port];
	devices[port] = &ioWatch;
}

void MSXCPUInterface::unregisterIOWatch(WatchPoint& watchPoint, MSXDevice** devices)
{
	assert(dynamic_cast<MSXWatchIODevice*>(&watchPoint));
	MSXWatchIODevice& ioWatch = static_cast<MSXWatchIODevice&>(watchPoint);
	unsigned port = ioWatch.getAddress();
	assert(port < 256);
	devices[port] = ioWatch.getDevicePtr();
}

void MSXCPUInterface::updateMemWatch(WatchPoint::Type type)
{
	std::bitset<CPU::CACHE_LINE_SIZE>* watchSet =
		(type == WatchPoint::READ_MEM) ? readWatchSet : writeWatchSet;
	for (int i = 0; i < CPU::CACHE_LINE_NUM; ++i) {
		watchSet[i].reset();
	}
	for (WatchPoints::const_iterator it = watchPoints.begin();
	     it != watchPoints.end(); ++it) {
		if ((*it)->getType() == type) {
			unsigned addr = (*it)->getAddress();
			assert(addr < 0x10000);
			watchSet[addr >> CPU::CACHE_LINE_BITS].set(
			         addr  & CPU::CACHE_LINE_LOW);
		}
	}
	setAllowedCache();
	msxcpu.invalidateMemCache(0x0000, 0x10000);
}

void MSXCPUInterface::executeMemWatch(word address, WatchPoint::Type type)
{
	for (WatchPoints::const_iterator it = watchPoints.begin();
	     it != watchPoints.end(); ++it) {
		if (((*it)->getAddress() == address) && ((*it)->getType() == type)) {
			(*it)->checkAndExecute();
		}
	}
}

// class MemoryDebug

MemoryDebug::MemoryDebug(
		MSXCPUInterface& interface_, MSXMotherBoard& motherBoard)
	: SimpleDebuggable(motherBoard, "memory",
	                   "The memory currently visible for the CPU.", 0x10000)
	, interface(interface_)
{
}

byte MemoryDebug::read(unsigned address, const EmuTime& time)
{
	return interface.peekMem(address, time);
}

void MemoryDebug::write(unsigned address, byte value,
                                         const EmuTime& time)
{
	return interface.writeMem(address, value, time);
}


// class SlottedMemoryDebug

SlottedMemoryDebug::SlottedMemoryDebug(
		MSXCPUInterface& interface_, MSXMotherBoard& motherBoard)
	: SimpleDebuggable(motherBoard, "slotted memory",
	                   "The memory in slots and subslots.", 0x10000 * 4 * 4)
	, interface(interface_)
{
}

byte SlottedMemoryDebug::read(unsigned address, const EmuTime& time)
{
	return interface.peekSlottedMem(address, time);
}

void SlottedMemoryDebug::write(unsigned address, byte value,
                                                const EmuTime& time)
{
	return interface.writeSlottedMem(address, value, time);
}


// class SubSlottedInfo

static unsigned getSlot(TclObject* token, const string& itemName)
{
	unsigned slot = token->getInt();
	if (slot >= 4) {
		throw CommandException(itemName + " must be in range 0..3");
	}
	return slot;
}

SlotInfo::SlotInfo(CommandController& commandController,
                   MSXCPUInterface& interface_)
	: InfoTopic(commandController, "slot")
	, interface(interface_)
{
}

void SlotInfo::execute(const std::vector<TclObject*>& tokens,
                       TclObject& result) const
{
	if (tokens.size() != 5) {
		throw SyntaxError();
	}
	unsigned ps   = getSlot(tokens[2], "Primary slot");
	unsigned ss   = getSlot(tokens[3], "Secondary slot");
	unsigned page = getSlot(tokens[4], "Page");
	if (!interface.isExpanded(ps)) {
		ss = 0;
	}
	result.setString(interface.slotLayout[ps][ss][page]->getName());
}

std::string SlotInfo::help(const std::vector<std::string>& /*tokens*/) const
{
	return "Retrieve name of the device inserted in given "
	       "primary slot / secondary slot / page.";
}


// class SubSlottedInfo

SubSlottedInfo::SubSlottedInfo(CommandController& commandController,
                               MSXCPUInterface& interface_)
	: InfoTopic(commandController, "issubslotted")
	, interface(interface_)
{
}

void SubSlottedInfo::execute(const std::vector<TclObject*>& tokens,
                             TclObject& result) const
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	result.setInt(interface.isExpanded(getSlot(tokens[2], "Slot")));
}

std::string SubSlottedInfo::help(
	const std::vector<std::string>& /*tokens*/) const
{
	return "Indicates whether a certain primary slot is expanded.";
}


// class ExternalSlotInfo

ExternalSlotInfo::ExternalSlotInfo(CommandController& commandController,
                                   CartridgeSlotManager& manager_)
	: InfoTopic(commandController, "isexternalslot")
	, manager(manager_)
{
}

void ExternalSlotInfo::execute(const std::vector<TclObject*>& tokens,
                               TclObject& result) const
{
	int ps = 0;
	int ss = 0;
	switch (tokens.size()) {
	case 4:
		ss = getSlot(tokens[3], "Secondary slot");
		// Fall-through
	case 3:
		ps = getSlot(tokens[2], "Primary slot");
		break;
	default:
		throw SyntaxError();
	}
	result.setInt(manager.isExternalSlot(ps, ss));
}

std::string ExternalSlotInfo::help(
	const std::vector<std::string>& /*tokens*/) const
{
	return "Indicates whether a certain slot is external or internal.";
}


// class IODebug

IODebug::IODebug(MSXCPUInterface& interface_,
                 MSXMotherBoard& motherBoard)
	: SimpleDebuggable(motherBoard, "ioports", "IO ports.", 0x100)
	, interface(interface_)
{
}

byte IODebug::read(unsigned address, const EmuTime& time)
{
	return interface.IO_In[address & 0xFF]->peekIO(address, time);
}

void IODebug::write(unsigned address, byte value, const EmuTime& time)
{
	interface.writeIO((word)address, value, time);
}


// class IOInfo

IOInfo::IOInfo(CommandController& commandController,
               MSXCPUInterface& interface_, bool input_)
	: InfoTopic(commandController, input_ ? "input_port" : "output_port")
	, interface(interface_), input(input_)
{
}

void IOInfo::execute(const vector<TclObject*>& tokens,
                     TclObject& result) const
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	unsigned port = tokens[2]->getInt();
	if (port >= 256) {
		throw CommandException("Port must be in range 0..255");
	}
	MSXDevice** devices = input ? interface.IO_In : interface.IO_Out;
	result.setString(devices[port]->getName());
}

string IOInfo::help(const vector<string>& /*tokens*/) const
{
	return "Return the name of the device connected to the given IO port.";
}


// class TurborCPUInterface

TurborCPUInterface::TurborCPUInterface(MSXMotherBoard& motherBoard)
	: MSXCPUInterface(motherBoard)
{
	delayDevice = DeviceFactory::createVDPIODelay(motherBoard);

	for (int port = 0x98; port <= 0x9B; ++port) {
		assert(IO_In [port] == &dummyDevice);
		assert(IO_Out[port] == &dummyDevice);
		IO_In [port] = delayDevice.get();
		IO_Out[port] = delayDevice.get();
	}
}

TurborCPUInterface::~TurborCPUInterface()
{
	for (int port = 0x98; port <= 0x9B; ++port) {
		assert(IO_In [port] == delayDevice.get());
		assert(IO_Out[port] == delayDevice.get());
		IO_In [port] = &dummyDevice;
		IO_Out[port] = &dummyDevice;
	}
}

} // namespace openmsx
