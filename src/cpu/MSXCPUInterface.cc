// $Id$

#include "MSXCPUInterface.hh"
#include "DummyDevice.hh"
#include "SimpleDebuggable.hh"
#include "InfoTopic.hh"
#include "CommandException.hh"
#include "TclObject.hh"
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
#include "serialize.hh"
#include "StringOp.hh"
#include "checked_cast.hh"
#include <cstdio>
#include <memory> // for auto_ptr
#include <set>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>
#include <cstring>

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
	virtual byte read(unsigned address, EmuTime::param time);
	virtual void write(unsigned address, byte value, EmuTime::param time);
private:
	MSXCPUInterface& interface;
};

class SlottedMemoryDebug : public SimpleDebuggable
{
public:
	SlottedMemoryDebug(MSXCPUInterface& interface,
	                   MSXMotherBoard& motherBoard);
	virtual byte read(unsigned address, EmuTime::param time);
	virtual void write(unsigned address, byte value, EmuTime::param time);
private:
	MSXCPUInterface& interface;
};

class IODebug : public SimpleDebuggable
{
public:
	IODebug(MSXCPUInterface& interface,
	        MSXMotherBoard& motherBoard);
	virtual byte read(unsigned address, EmuTime::param time);
	virtual void write(unsigned address, byte value, EmuTime::param time);
private:
	MSXCPUInterface& interface;
};

class SlotInfo : public InfoTopic
{
public:
	SlotInfo(InfoCommand& machineInfoCommand,
	         MSXCPUInterface& interface);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
private:
	MSXCPUInterface& interface;
};

class SubSlottedInfo : public InfoTopic
{
public:
	SubSlottedInfo(InfoCommand& machineInfoCommand,
		       MSXCPUInterface& interface);
	virtual void execute(const vector<TclObject*>& tokens,
			     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
private:
	MSXCPUInterface& interface;
};

class ExternalSlotInfo : public InfoTopic
{
public:
	ExternalSlotInfo(InfoCommand& machineInfoCommand,
			 CartridgeSlotManager& manager);
	virtual void execute(const vector<TclObject*>& tokens,
			     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
private:
	CartridgeSlotManager& manager;
};

class IOInfo : public InfoTopic
{
public:
	IOInfo(InfoCommand& machineInfoCommand,
	       MSXCPUInterface& interface, bool input);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
private:
	MSXCPUInterface& interface;
	bool input;
};


// Global variables
bool MSXCPUInterface::breaked = false;
bool MSXCPUInterface::continued = false;
bool MSXCPUInterface::step = false;


// Bitfields used in the disallowReadCache and disallowWriteCache arrays
static const byte SECUNDARY_SLOT_BIT = 0x01;
static const byte MEMORY_WATCH_BIT   = 0x02;
static const byte GLOBAL_WRITE_BIT   = 0x04;


MSXCPUInterface::MSXCPUInterface(MSXMotherBoard& motherBoard_)
	: memoryDebug       (new MemoryDebug       (*this, motherBoard_))
	, slottedMemoryDebug(new SlottedMemoryDebug(*this, motherBoard_))
	, ioDebug           (new IODebug           (*this, motherBoard_))
	, slotInfo        (new SlotInfo(
		motherBoard_.getMachineInfoCommand(), *this))
	, subSlottedInfo  (new SubSlottedInfo(
		motherBoard_.getMachineInfoCommand(), *this))
	, externalSlotInfo(new ExternalSlotInfo(
		motherBoard_.getMachineInfoCommand(),
		motherBoard_.getSlotManager()))
	, inputPortInfo (new IOInfo(
	        motherBoard_.getMachineInfoCommand(), *this, true))
	, outputPortInfo(new IOInfo(
	        motherBoard_.getMachineInfoCommand(), *this, false))
	, dummyDevice(motherBoard_.getDummyDevice())
	, msxcpu(motherBoard_.getCPU())
	, cliComm(motherBoard_.getMSXCliComm())
	, motherBoard(motherBoard_)
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

	// initially allow all regions to be cached
	memset(disallowReadCache,  0, sizeof(disallowReadCache));
	memset(disallowWriteCache, 0, sizeof(disallowWriteCache));

	// Note: SlotState is initialised at reset

	msxcpu.setInterface(this);

	if (motherBoard.isTurboR()) {
		// TODO also MSX2+ needs (slightly different) VDPIODelay
		delayDevice = DeviceFactory::createVDPIODelay(motherBoard);
		for (int port = 0x98; port <= 0x9B; ++port) {
			assert(IO_In [port] == &dummyDevice);
			assert(IO_Out[port] == &dummyDevice);
			IO_In [port] = delayDevice.get();
			IO_Out[port] = delayDevice.get();
		}
	}
}

MSXCPUInterface::~MSXCPUInterface()
{
	removeAllWatchPoints();

	if (delayDevice.get()) {
		for (int port = 0x98; port <= 0x9B; ++port) {
			assert(IO_In [port] == delayDevice.get());
			assert(IO_Out[port] == delayDevice.get());
			IO_In [port] = &dummyDevice;
			IO_Out[port] = &dummyDevice;
		}
	}

	msxcpu.setInterface(NULL);

	#ifndef NDEBUG
	for (int port = 0; port < 256; ++port) {
		if (IO_In[port] != &dummyDevice) {
			std::cout << "In-port " << port << " still registered "
			          << IO_In[port]->getName() << std::endl;
			assert(false);
		}
		if (IO_Out[port] != &dummyDevice) {
			std::cout << "Out-port " << port << " still registered "
			          << IO_Out[port]->getName() << std::endl;
			assert(false);
		}
	}
	for (int primSlot = 0; primSlot < 4; ++primSlot) {
		assert(!isExpanded(primSlot));
		for (int secSlot = 0; secSlot < 4; ++secSlot) {
			for (int page = 0; page < 4; ++page) {
				assert(slotLayout[primSlot][secSlot][page] == &dummyDevice);
			}
		}
	}
	#endif
}

void MSXCPUInterface::removeAllWatchPoints()
{
	while (!watchPoints.empty()) {
		removeWatchPoint(*watchPoints.back());
	}

}

byte MSXCPUInterface::readMemSlow(word address, EmuTime::param time)
{
	// something special in this region?
	if (unlikely(disallowReadCache[address >> CacheLine::BITS])) {
		// execute read watches before actual read
		if (readWatchSet[address >> CacheLine::BITS]
		                [address &  CacheLine::LOW]) {
			executeMemWatch(address, WatchPoint::READ_MEM);
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
	} else {
		visibleDevices[address>>14]->writeMem(address, value, time);
	}
	// something special in this region?
	if (unlikely(disallowWriteCache[address >> CacheLine::BITS])) {
		// slot-select-ignore writes (Super Lode Runner)
		for (GlobalWrites::const_iterator it = globalWrites.begin();
		     it != globalWrites.end(); ++it) {
			// very primitive address selection mechanism,
			// but more than enough for now
			if (unlikely(it->addr == address)) {
				it->device->globalWrite(address, value, time);
			}
		}
		// execute write watches after actual write
		if (writeWatchSet[address >> CacheLine::BITS]
		                 [address &  CacheLine::LOW]) {
			executeMemWatch(address, WatchPoint::WRITE_MEM);
		}
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
		int ps, vector<MSXDevice*>& alreadyRemoved) const
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
			cliComm.printWarning(
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

void MSXCPUInterface::registerGlobalWrite(MSXDevice& device, word address)
{
	GlobalWriteInfo info = { &device, address };
	globalWrites.push_back(info);

	disallowWriteCache[address >> CacheLine::BITS] |= GLOBAL_WRITE_BIT;
	msxcpu.invalidateMemCache(address & CacheLine::HIGH, 0x100);
}

void MSXCPUInterface::unregisterGlobalWrite(MSXDevice& device, word address)
{
	GlobalWriteInfo info = { &device, address };
	GlobalWrites::iterator it =
		find(globalWrites.begin(), globalWrites.end(), info);
	assert(it != globalWrites.end());
	globalWrites.erase(it);

	for (GlobalWrites::const_iterator it = globalWrites.begin();
	     it != globalWrites.end(); ++it) {
		if ((it->addr >> CacheLine::BITS) ==
		    (address  >> CacheLine::BITS)) {
			// there is still a global write in this region
			return;
		}
	}
	disallowWriteCache[address >> CacheLine::BITS] &= ~GLOBAL_WRITE_BIT;
	msxcpu.invalidateMemCache(address & CacheLine::HIGH, 0x100);
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

byte MSXCPUInterface::readIRQVector()
{
	return motherBoard.readIRQVector();
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
	if (isExpanded(primarySlotState[3])) {
		disallowReadCache [0xFF] |=  SECUNDARY_SLOT_BIT;
		disallowWriteCache[0xFF] |=  SECUNDARY_SLOT_BIT;
	} else {
		disallowReadCache [0xFF] &= ~SECUNDARY_SLOT_BIT;
		disallowWriteCache[0xFF] &= ~SECUNDARY_SLOT_BIT;
	}
	msxcpu.invalidateMemCache(0xFFFF & CacheLine::HIGH, 0x100);
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


void MSXCPUInterface::setWatchPoint(auto_ptr<WatchPoint> watchPoint_)
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
	unsigned beginPort = ioWatch.getBeginAddress();
	unsigned endPort   = ioWatch.getEndAddress();
	assert(beginPort <= endPort);
	assert(endPort < 0x100);
	for (unsigned port = beginPort; port <= endPort; ++port) {
		ioWatch.getDevicePtr() = devices[port];
		devices[port] = &ioWatch;
	}
}

void MSXCPUInterface::unregisterIOWatch(WatchPoint& watchPoint, MSXDevice** devices)
{
	const MSXWatchIODevice& ioWatch =
		checked_cast<const MSXWatchIODevice&>(watchPoint);
	unsigned beginPort = ioWatch.getBeginAddress();
	unsigned endPort   = ioWatch.getEndAddress();
	assert(beginPort <= endPort);
	assert(endPort < 0x100);

	for (unsigned port = beginPort; port <= endPort; ++port) {
		// find pointer to watchpoint
		MSXDevice** prev = &devices[port];
		while (*prev != &ioWatch) {
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
	for (WatchPoints::const_iterator it = watchPoints.begin();
	     it != watchPoints.end(); ++it) {
		if ((*it)->getType() == type) {
			unsigned beginAddr = (*it)->getBeginAddress();
			unsigned endAddr   = (*it)->getEndAddress();
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

void MSXCPUInterface::executeMemWatch(word address, WatchPoint::Type type)
{
	for (WatchPoints::const_iterator it = watchPoints.begin();
	     it != watchPoints.end(); ++it) {
		if (((*it)->getBeginAddress() <= address) &&
		    ((*it)->getEndAddress()   >= address) &&
		    ((*it)->getType()         == type)) {
			(*it)->checkAndExecute();
		}
	}
}


void MSXCPUInterface::doBreak()
{
	if (breaked) return;
	breaked = true;

	Reactor& reactor = motherBoard.getReactor();
	reactor.block();
	reactor.getCliComm().update(CliComm::STATUS, "cpu", "suspended");
	reactor.getEventDistributor().distributeEvent(
		new SimpleEvent<OPENMSX_BREAK_EVENT>());
}

void MSXCPUInterface::doStep()
{
	if (breaked) {
		step = true;
		doContinue2();
	}
}

void MSXCPUInterface::doContinue()
{
	if (breaked) {
		continued = true;
		doContinue2();
	}
}

void MSXCPUInterface::doContinue2()
{
	breaked = false;
	Reactor& reactor = motherBoard.getReactor();
	reactor.getCliComm().update(CliComm::STATUS, "cpu", "running");
	reactor.unblock();
}


// class MemoryDebug

MemoryDebug::MemoryDebug(
		MSXCPUInterface& interface_, MSXMotherBoard& motherBoard)
	: SimpleDebuggable(motherBoard, "memory",
	                   "The memory currently visible for the CPU.", 0x10000)
	, interface(interface_)
{
}

byte MemoryDebug::read(unsigned address, EmuTime::param time)
{
	return interface.peekMem(address, time);
}

void MemoryDebug::write(unsigned address, byte value,
                                         EmuTime::param time)
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

byte SlottedMemoryDebug::read(unsigned address, EmuTime::param time)
{
	return interface.peekSlottedMem(address, time);
}

void SlottedMemoryDebug::write(unsigned address, byte value,
                                                EmuTime::param time)
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

SlotInfo::SlotInfo(InfoCommand& machineInfoCommand,
                   MSXCPUInterface& interface_)
	: InfoTopic(machineInfoCommand, "slot")
	, interface(interface_)
{
}

void SlotInfo::execute(const vector<TclObject*>& tokens,
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

string SlotInfo::help(const vector<string>& /*tokens*/) const
{
	return "Retrieve name of the device inserted in given "
	       "primary slot / secondary slot / page.";
}


// class SubSlottedInfo

SubSlottedInfo::SubSlottedInfo(InfoCommand& machineInfoCommand,
                               MSXCPUInterface& interface_)
	: InfoTopic(machineInfoCommand, "issubslotted")
	, interface(interface_)
{
}

void SubSlottedInfo::execute(const vector<TclObject*>& tokens,
                             TclObject& result) const
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	result.setInt(interface.isExpanded(getSlot(tokens[2], "Slot")));
}

string SubSlottedInfo::help(
	const vector<string>& /*tokens*/) const
{
	return "Indicates whether a certain primary slot is expanded.";
}


// class ExternalSlotInfo

ExternalSlotInfo::ExternalSlotInfo(InfoCommand& machineInfoCommand,
                                   CartridgeSlotManager& manager_)
	: InfoTopic(machineInfoCommand, "isexternalslot")
	, manager(manager_)
{
}

void ExternalSlotInfo::execute(const vector<TclObject*>& tokens,
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
	result.setInt(manager.isExternalSlot(ps, ss, true));
}

string ExternalSlotInfo::help(
	const vector<string>& /*tokens*/) const
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

byte IODebug::read(unsigned address, EmuTime::param time)
{
	return interface.IO_In[address & 0xFF]->peekIO(address, time);
}

void IODebug::write(unsigned address, byte value, EmuTime::param time)
{
	interface.writeIO(word(address), value, time);
}


// class IOInfo

IOInfo::IOInfo(InfoCommand& machineInfoCommand,
               MSXCPUInterface& interface_, bool input_)
	: InfoTopic(machineInfoCommand, input_ ? "input_port" : "output_port")
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

	if (delayDevice.get()) {
		ar.serialize("vdpDelay", *delayDevice);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXCPUInterface);

} // namespace openmsx
