// $Id$

#include "MSXCPUInterface.hh"
#include "DummyDevice.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "XMLElement.hh"
#include "VDPIODelay.hh"
#include "CliComm.hh"
#include "MSXMultiIODevice.hh"
#include "MSXMultiMemDevice.hh"
#include "MSXException.hh"
#include "CartridgeSlotManager.hh"
#include <cstdio>
#include <memory> // for auto_ptr
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
	: memoryDebug       (*this, motherBoard)
	, slottedMemoryDebug(*this, motherBoard)
	, ioDebug           (*this, motherBoard)
	, subSlottedInfo  (motherBoard.getCommandController(), *this)
	, externalSlotInfo(motherBoard.getCommandController(),
	                   motherBoard.getSlotManager())
	, slotMapCmd      (motherBoard.getCommandController(), *this)
	, ioMapCmd        (motherBoard.getCommandController(), *this)
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

	// Note: SlotState is initialised at reset

	msxcpu.setInterface(this);
}

MSXCPUInterface::~MSXCPUInterface()
{
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

void MSXCPUInterface::unsetExpanded(int ps)
{
	assert(isExpanded(ps));
	if (expanded[ps] == 1) {
		for (int ss = 0; ss < 4; ++ss) {
			for (int page = 0; page < 4; ++page) {
				if (slotLayout[ps][ss][page] != &dummyDevice) {
					throw MSXException(
						"Can't remove slotexpander "
						"because slot is still in use.");
				}
			}
		}
	}
	expanded[ps]--;
}

void MSXCPUInterface::register_IO_In(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = (IO_In[port] == delayDevice.get())
	                      ? delayDevice->getInDevicePtr(port)
	                      : IO_In[port];
	register_IO(port, true, devicePtr, device); // in
}

void MSXCPUInterface::unregister_IO_In(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = (IO_In[port] == delayDevice.get())
	                      ? delayDevice->getInDevicePtr(port)
	                      : IO_In[port];
	unregister_IO(devicePtr, device);
}

void MSXCPUInterface::register_IO_Out(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = (IO_Out[port] == delayDevice.get())
	                      ? delayDevice->getOutDevicePtr(port)
	                      : IO_Out[port];
	register_IO(port, false, devicePtr, device); // out
}

void MSXCPUInterface::unregister_IO_Out(byte port, MSXDevice* device)
{
	MSXDevice*& devicePtr = (IO_Out[port] == delayDevice.get())
	                      ? delayDevice->getOutDevicePtr(port)
	                      : IO_Out[port];
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
	throw FatalError("Overlapping memory devices in slot " +
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
	MSXDevice*& slot = slotLayout[ps][ss][base >> 14];
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
}

void MSXCPUInterface::unregisterSlot(
	MSXDevice& device, int ps, int ss, int base, int size)
{
	MSXDevice*& slot = slotLayout[ps][ss][base >> 14];
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

string MSXCPUInterface::getSlotMap() const
{
	ostringstream out;
	for (int prim = 0; prim < 4; ++prim) {
		if (isExpanded(prim)) {
			for (int sec = 0; sec < 4; ++sec) {
				out << "slot " << prim << "." << sec << ":\n";
				printSlotMapPages(out, slotLayout[prim][sec]);
			}
		} else {
			out << "slot " << prim << ":\n";
			printSlotMapPages(out, slotLayout[prim][0]);
		}
	}
	return out.str();
}

static void ioMapHelper(ostringstream& out,
                        const string& type, int begin, int end,
                        const MSXDevice* device)
{
	out << "port " << std::hex << std::setw(2) << std::setfill('0')
	    << std::uppercase << begin;
	if (begin == end - 1) {
		out << ":    ";
	} else {
		out << "-" << setw(2) << setfill('0') << uppercase << end - 1 << ": ";
	}
	out << type << " " << device->getName() << std::endl;
}

string MSXCPUInterface::getIOMap() const
{
	ostringstream result;
	int port = 0;
	while (port < 256) {
		const MSXDevice* deviceIn  = IO_In [port];
		const MSXDevice* deviceOut = IO_Out[port];

		// Scan over equal region.
		int endPort = port + 1; // exclusive
		while (endPort < 256
		    && IO_In [endPort] == deviceIn
		    && IO_Out[endPort] == deviceOut) ++endPort;

		// Print device(s), except empty regions.
		if (deviceIn == deviceOut) {
			if (deviceIn != &dummyDevice) {
				ioMapHelper(result, "I/O", port, endPort, deviceIn);
			}
		} else {
			if (deviceIn != &dummyDevice) {
				ioMapHelper(result, "I  ", port, endPort, deviceIn);
			}
			if (deviceOut != &dummyDevice) {
				ioMapHelper(result, "  O", port, endPort, deviceOut);
			}
		}

		port = endPort;
	}
	return result.str();
}

void MSXCPUInterface::printSlotMapPages(std::ostream &out,
	const MSXDevice* const* devices) const
{
	for (int page = 0; page < 4; ++page) {
		char hexStr[5];
		snprintf(hexStr, sizeof(hexStr), "%04X", page * 0x4000);
		out << hexStr << ": " << devices[page]->getName() << "\n";
	}
}


// class MemoryDebug

MSXCPUInterface::MemoryDebug::MemoryDebug(
		MSXCPUInterface& interface_, MSXMotherBoard& motherBoard)
	: SimpleDebuggable(motherBoard, "memory",
	                   "The memory currently visible for the CPU.", 0x10000)
	, interface(interface_)
{
}

byte MSXCPUInterface::MemoryDebug::read(unsigned address, const EmuTime& time)
{
	return interface.peekMem(address, time);
}

void MSXCPUInterface::MemoryDebug::write(unsigned address, byte value,
                                         const EmuTime& time)
{
	return interface.writeMem(address, value, time);
}


// class SlottedMemoryDebug

MSXCPUInterface::SlottedMemoryDebug::SlottedMemoryDebug(
		MSXCPUInterface& interface_, MSXMotherBoard& motherBoard)
	: SimpleDebuggable(motherBoard, "slotted memory",
	                   "The memory in slots and subslots.", 0x10000 * 4 * 4)
	, interface(interface_)
{
}

byte MSXCPUInterface::SlottedMemoryDebug::read(unsigned address, const EmuTime& time)
{
	return interface.peekSlottedMem(address, time);
}

void MSXCPUInterface::SlottedMemoryDebug::write(unsigned address, byte value,
                                                const EmuTime& time)
{
	return interface.writeSlottedMem(address, value, time);
}


// class SubSlottedInfo

static unsigned getSlot(TclObject* token)
{
	unsigned slot = token->getInt();
	if (slot >= 4) {
		throw CommandException("Slot must be in range 0..3");
	}
	return slot;
}

MSXCPUInterface::SubSlottedInfo::SubSlottedInfo(
		CommandController& commandController,
		MSXCPUInterface& interface_)
	: InfoTopic(commandController, "issubslotted")
	, interface(interface_)
{
}

void MSXCPUInterface::SubSlottedInfo::execute(
	const std::vector<TclObject*>& tokens,
	TclObject& result) const
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	result.setInt(interface.isExpanded(getSlot(tokens[2])));
}

std::string MSXCPUInterface::SubSlottedInfo::help(
	const std::vector<std::string>& /*tokens*/) const
{
	return "Indicates whether a certain primary slot is expanded.";
}


// class ExternalSlotInfo

MSXCPUInterface::ExternalSlotInfo::ExternalSlotInfo(
		CommandController& commandController,
		CartridgeSlotManager& manager_)
	: InfoTopic(commandController, "isexternalslot")
	, manager(manager_)
{
}

void MSXCPUInterface::ExternalSlotInfo::execute(
	const std::vector<TclObject*>& tokens,
	TclObject& result) const
{
	int ps = 0;
	int ss = 0;
	switch (tokens.size()) {
	case 4:
		ss = getSlot(tokens[3]);
		// Fall-through
	case 3:
		ps = getSlot(tokens[2]);
		break;
	default:
		throw SyntaxError();
	}
	result.setInt(manager.isExternalSlot(ps, ss));
}

std::string MSXCPUInterface::ExternalSlotInfo::help(
	const std::vector<std::string>& /*tokens*/) const
{
	return "Indicates whether a certain slot is external or internal.";
}


// class IODebug

MSXCPUInterface::IODebug::IODebug(MSXCPUInterface& interface_,
                                  MSXMotherBoard& motherBoard)
	: SimpleDebuggable(motherBoard, "ioports", "IO ports.", 0x100)
	, interface(interface_)
{
}

byte MSXCPUInterface::IODebug::read(unsigned address, const EmuTime& time)
{
	return interface.IO_In[address & 0xFF]->peekIO(address, time);
}

void MSXCPUInterface::IODebug::write(unsigned address, byte value, const EmuTime& time)
{
	interface.writeIO((word)address, value, time);
}


// class SlotMapCmd

MSXCPUInterface::SlotMapCmd::SlotMapCmd(
		CommandController& commandController,
		MSXCPUInterface& interface_)
	: SimpleCommand(commandController, "slotmap")
	, interface(interface_)
{
}

string MSXCPUInterface::SlotMapCmd::execute(const vector<string>& /*tokens*/)
{
	return interface.getSlotMap();
}

string MSXCPUInterface::SlotMapCmd::help(const vector<string>& /*tokens*/) const
{
	return "Prints which slots contain which devices.\n";
}


// class IOMapCmd

MSXCPUInterface::IOMapCmd::IOMapCmd(
		CommandController& commandController,
		MSXCPUInterface& interface_)
	: SimpleCommand(commandController, "iomap")
	, interface(interface_)
{
}

string MSXCPUInterface::IOMapCmd::execute(const vector<string>& /*tokens*/)
{
	return interface.getIOMap();
}

string MSXCPUInterface::IOMapCmd::help(const vector<string>& /*tokens*/) const
{
	return "Prints which I/O ports are connected to which devices.\n";
}

// class TurborCPUInterface

TurborCPUInterface::TurborCPUInterface(MSXMotherBoard& motherBoard)
	: MSXCPUInterface(motherBoard)
{
	delayDevice.reset(new VDPIODelay(motherBoard, EmuTime::zero));

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
