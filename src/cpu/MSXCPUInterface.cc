// $Id$

#include <cstdio>
#include <memory> // for auto_ptr
#include <sstream>
#include "MSXCPUInterface.hh"
#include "DummyDevice.hh"
#include "CommandController.hh"
#include "MSXCPU.hh"
#include "Scheduler.hh"
#include "HardwareConfig.hh"
#include "Config.hh"
#include "CartridgeSlotManager.hh"
#include "VDPIODelay.hh"
#include "Debugger.hh"
#include "CliCommOutput.hh"
#include "MSXMultiIODevice.hh"

using std::auto_ptr;
using std::ostringstream;

namespace openmsx {

MSXCPUInterface& MSXCPUInterface::instance()
{
	static auto_ptr<MSXCPUInterface> oneInstance;
	if (!oneInstance.get()) {
		// TODO choose one depending on MSX model (small optimization)
		//oneInstance.reset(new MSXCPUInterface());
		oneInstance.reset(new TurborCPUInterface());
	}
	return *oneInstance.get();
}

MSXCPUInterface::MSXCPUInterface()
	: memoryDebug(*this),
	  slottedMemoryDebug(*this),
	  ioDebug(*this),
	  slotMapCmd(*this),
	  slotSelectCmd(*this),
	  dummyDevice(DummyDevice::instance()),
	  hardwareConfig(HardwareConfig::instance()),
	  commandController(CommandController::instance()),
	  msxcpu(MSXCPU::instance()),
	  scheduler(Scheduler::instance()),
	  slotManager(CartridgeSlotManager::instance()),
	  debugger(Debugger::instance()),
	  cliCommOutput(CliCommOutput::instance())
{
	for (int port = 0; port < 256; ++port) {
		IO_In [port] = &dummyDevice;
		IO_Out[port] = &dummyDevice;
	}
	for (int primSlot = 0; primSlot < 4; ++primSlot) {
		primarySlotState[primSlot] = 0;
		isSubSlotted[primSlot] = false;
		subSlotRegister[primSlot] = 0;
		for (int secSlot = 0; secSlot < 4; ++secSlot) {
			for (int page = 0; page < 4; ++page) {
				slotLayout[primSlot][secSlot][page] = &dummyDevice;
			}
		}
	}
	for (int page = 0; page < 4; ++page) {
		visibleDevices[page] = 0;
	}

	Config* config = hardwareConfig.getConfigById("MotherBoard");
	Config::Children slots;
	config->getChildren("slot", slots);
	for (Config::Children::const_iterator it = slots.begin();
	     it != slots.end(); ++it) {
		unsigned num = StringOp::stringToInt((*it)->getAttribute("num"));
		bool expanded = StringOp::stringToBool((*it)->getAttribute("expanded"));
		if (num >= 4) {
			throw FatalError("Invalid slot specification");
		}
		isSubSlotted[num] = expanded;
	}

	// Note: SlotState is initialised at reset

	// Register console commands
	commandController.registerCommand(&slotMapCmd,    "slotmap");
	commandController.registerCommand(&slotSelectCmd, "slotselect");

	debugger.registerDebuggable("memory", memoryDebug);
	debugger.registerDebuggable("slotted memory", slottedMemoryDebug);
	debugger.registerDebuggable("I/O ports", ioDebug);

	msxcpu.setInterface(this);
}

MSXCPUInterface::~MSXCPUInterface()
{
	msxcpu.setInterface(NULL);

	debugger.unregisterDebuggable("memory", memoryDebug);
	debugger.unregisterDebuggable("slotted memory", slottedMemoryDebug);
	debugger.unregisterDebuggable("I/O ports", ioDebug);

	commandController.unregisterCommand(&slotMapCmd,    "slotmap");
	commandController.unregisterCommand(&slotSelectCmd, "slotselect");

	for (set<byte>::const_iterator it = multiIn.begin();
	     it != multiIn.end(); ++it) {
		delete IO_In[*it];
	}
	for (set<byte>::const_iterator it = multiOut.begin();
	     it != multiOut.end(); ++it) {
		delete IO_Out[*it];
	}
}


void MSXCPUInterface::register_IO_In(byte port, MSXIODevice *device)
{
	PRT_DEBUG(device->getName() << " registers In-port " <<
	          hex << (int)port << dec);
	if (IO_In[port] == &dummyDevice) {
		// first 
		IO_In[port] = device;
	} else {
		MSXIODevice* dev2 = IO_In[port];
		if (multiIn.find(port) == multiIn.end()) {
			// second
			MSXMultiIODevice* multi = new MSXMultiIODevice();
			multiIn.insert(port);
			multi->addDevice(dev2);
			multi->addDevice(device);
			IO_In[port] = multi;
			dev2 = multi;
		} else {
			// third or more
			static_cast<MSXMultiIODevice*>(dev2)->addDevice(device);
		}
		ostringstream os;
		os << "Conflicting input port 0x" << hex << (int)port
		   << "for devices " << dev2->getName();
		cliCommOutput.printWarning(os.str());
	}
}

void MSXCPUInterface::register_IO_Out(byte port, MSXIODevice *device)
{
	PRT_DEBUG(device->getName() << " registers Out-port " <<
	          hex << (int)port << dec);
	if (IO_Out[port] == &dummyDevice) {
		// first
		IO_Out[port] = device;
	} else {
		MSXIODevice* dev2 = IO_Out[port];
		if (multiOut.find(port) == multiOut.end()) {
			// second
			MSXMultiIODevice* multi = new MSXMultiIODevice();
			multiOut.insert(port);
			multi->addDevice(dev2);
			multi->addDevice(device);
			IO_Out[port] = multi;
		} else {
			// third or more
			static_cast<MSXMultiIODevice*>(dev2)->addDevice(device);
		}
	}
}

void MSXCPUInterface::registerSlot(MSXMemDevice *device,
                                   int primSl, int secSl, int page)
{
	if (!isSubSlotted[primSl] && secSl != 0) {
		ostringstream s;
		s << "slot " << primSl << "." << secSl
			<< " does not exist, because slot is not expanded";
		throw MSXException(s.str());
	}
	if (slotLayout[primSl][secSl][page] == &dummyDevice) {
		PRT_DEBUG(device->getName() << " registers at "<<primSl<<" "<<secSl<<" "<<page);
		slotLayout[primSl][secSl][page] = device;
	} else {
		throw FatalError("Device: \"" + device->getName() +
		                 "\" trying to register taken slot");
	}
}

void MSXCPUInterface::registerSlottedDevice(MSXMemDevice* device,
                                            int primSl, int secSl, int pages)
{
	for (int i = 0; i < 4; i++) {
		if (pages & (1 << i)) {
			registerSlot(device, primSl, secSl, i);
		}
	}
}

void MSXCPUInterface::registerSlottedDevice(MSXMemDevice* device, int pages)
{
	RegPostSlot regPostSlot(device, pages);
	regPostSlots.push_back(regPostSlot);
}

void MSXCPUInterface::registerPostSlots()
{
	for (vector<RegPostSlot>::const_iterator it = regPostSlots.begin();
	     it != regPostSlots.end();
	     ++it) {
		int ps, ss;
		slotManager.getSlot(ps, ss);
		registerSlottedDevice(it->device, ps, ss, it->pages);
	}
}

void MSXCPUInterface::updateVisible(int page)
{
	MSXMemDevice *newDevice = slotLayout [primarySlotState[page]]
	                                     [secondarySlotState[page]]
	                                     [page];
	if (visibleDevices[page] != newDevice) {
		visibleDevices[page] = newDevice;
		// Different device, so cache is no longer valid.
		msxcpu.invalidateCache(page * 0x4000,
		                        0x4000 / CPU::CACHE_LINE_SIZE);
	}
	PRT_DEBUG(" page: " << (int)page <<
	          " ps: " << (int)primarySlotState[page] <<
	          " ss: " << (int)secondarySlotState[page] <<
	          " device: " << newDevice->getName());
}

void MSXCPUInterface::reset()
{
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


byte MSXCPUInterface::readMem(word address, const EmuTime &time)
{
	if ((address == 0xFFFF) && isSubSlotted[primarySlotState[3]]) {
		return 0xFF ^ subSlotRegister[primarySlotState[3]];
	} else {
		return visibleDevices[address >> 14]->readMem(address, time);
	}
}

void MSXCPUInterface::writeMem(word address, byte value, const EmuTime &time)
{
	if ((address == 0xFFFF) && isSubSlotted[primarySlotState[3]]) {
		setSubSlot(primarySlotState[3], value);
	} else {
		visibleDevices[address>>14]->writeMem(address, value, time);
	}
}

byte MSXCPUInterface::readIO(word prt, const EmuTime &time)
{
	byte port = (byte)prt;
	return IO_In[port]->readIO(port, time);
}

void MSXCPUInterface::writeIO(word prt, byte value, const EmuTime &time)
{
	byte port = (byte)prt;
	IO_Out[port]->writeIO(port, value, time);
}

const byte* MSXCPUInterface::getReadCacheLine(word start) const
{
	if ((start == 0x10000 - CPU::CACHE_LINE_SIZE) && // contains 0xffff
	    (isSubSlotted[primarySlotState[3]]))
		return NULL;
	return visibleDevices[start >> 14]->getReadCacheLine(start);
}

byte* MSXCPUInterface::getWriteCacheLine(word start) const
{
	if ((start == 0x10000 - CPU::CACHE_LINE_SIZE) && // contains 0xffff
	    (isSubSlotted[primarySlotState[3]]))
		return NULL;
	return visibleDevices[start >> 14]->getWriteCacheLine(start);
}

byte MSXCPUInterface::peekMem(word address) const
{
	if ((address == 0xFFFF) && isSubSlotted[primarySlotState[3]]) {
		return 0xFF ^ subSlotRegister[primarySlotState[3]];
	} else {
		return visibleDevices[address >> 14]->peekMem(address);
	}
}

byte MSXCPUInterface::peekSlottedMem(unsigned address) const
{
	byte primSlot = (address & 0xC0000) >> 18;
	byte subSlot = (address & 0x30000) >> 16;
	byte page = (address & 0x0C000) >> 14;
	word offset = (address & 0xFFFF); // includes page
	if (!isSubSlotted[primSlot]) {
		subSlot = 0;
	}

	if ((offset == 0xFFFF) && isSubSlotted[primSlot]) {
		return 0xFF ^ subSlotRegister[primSlot];
	} else {
		return slotLayout[primSlot][subSlot][page]->peekMem(offset);
	}
}

void MSXCPUInterface::writeSlottedMem(unsigned address, byte value,
                                      const EmuTime& time)
{
	byte primSlot = (address & 0xC0000) >> 18;
	byte subSlot = (address & 0x30000) >> 16;
	byte page = (address & 0x0C000) >> 14;
	word offset = (address & 0xFFFF); // includes page
	if (!isSubSlotted[primSlot]) {
		subSlot = 0;
	}

	if ((offset == 0xFFFF) && isSubSlotted[primSlot]) {
		setSubSlot(primSlot, value);
	} else {
		slotLayout[primSlot][subSlot][page]->writeMem(offset, value, time);
	}
}

string MSXCPUInterface::getSlotMap() const
{
	ostringstream out;
	for (int prim = 0; prim < 4; ++prim) {
		if (isSubSlotted[prim]) {
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

void MSXCPUInterface::printSlotMapPages(ostream &out,
	const MSXMemDevice* const* devices) const
{
	for (int page = 0; page < 4; ++page) {
		char hexStr[5];
		snprintf(hexStr, sizeof(hexStr), "%04X", page * 0x4000);
		out << hexStr << ": " << devices[page]->getName() << "\n";
	}
}

string MSXCPUInterface::getSlotSelection() const
{
	ostringstream out;
	for (int page = 0; page < 4; ++page) {
		char pageStr[5];
		snprintf(pageStr, sizeof(pageStr), "%04X", page * 0x4000);
		out << pageStr << ": ";

		int prim = primarySlotState[page];
		int sec = (subSlotRegister[prim] >> (page * 2)) & 3;
		if (isSubSlotted[prim]) {
			out << "slot " << prim << "." << sec << "\n";
		} else {
			out << "slot " << prim << "\n";
		}
	}
	return out.str();
}

MSXCPUInterface::SlotSelection* MSXCPUInterface::getCurrentSlots()
{
	MSXCPUInterface::SlotSelection * slots = new SlotSelection();
	for (int page = 0; page < 4; ++page) {
		slots->primary[page] = primarySlotState[page];
		slots->secondary[page] = (subSlotRegister[slots->primary[page]] >>
		                         (page * 2)) & 3;
		slots->isSubSlotted[page] = isSubSlotted[slots->primary[page]];
	}
	return slots;
}


// class MemoryDebug
 
MSXCPUInterface::MemoryDebug::MemoryDebug(MSXCPUInterface& parent_)
	: parent(parent_)
{
}

unsigned MSXCPUInterface::MemoryDebug::getSize() const
{
	return 0x10000;
}

const string& MSXCPUInterface::MemoryDebug::getDescription() const
{
	static const string desc =
		"The memory currently visible for the CPU.";
	return desc;
}

byte MSXCPUInterface::MemoryDebug::read(unsigned address)
{
	return parent.peekMem(address);
}

void MSXCPUInterface::MemoryDebug::write(unsigned address, byte value)
{
	const EmuTime& time = parent.scheduler.getCurrentTime();
	return parent.writeMem(address, value, time);
}


// class SlottedMemoryDebug
 
MSXCPUInterface::SlottedMemoryDebug::SlottedMemoryDebug(MSXCPUInterface& parent_)
	: parent(parent_)
{
}

unsigned MSXCPUInterface::SlottedMemoryDebug::getSize() const
{
	return 0x10000 * 4 * 4;
}

const string& MSXCPUInterface::SlottedMemoryDebug::getDescription() const
{
	static const string desc =
		"The memory in slots and subslots.";
	return desc;
}

byte MSXCPUInterface::SlottedMemoryDebug::read(unsigned address)
{
	return parent.peekSlottedMem(address);
}

void MSXCPUInterface::SlottedMemoryDebug::write(unsigned address, byte value)
{
	const EmuTime& time = parent.scheduler.getCurrentTime();
	return parent.writeSlottedMem(address, value, time);
}


// class IODebug

MSXCPUInterface::IODebug::IODebug(MSXCPUInterface& parent_)
	: parent(parent_)
{
}

unsigned MSXCPUInterface::IODebug::getSize() const
{
	return 0x100;
}

const string& MSXCPUInterface::IODebug::getDescription() const
{
	static const string desc =
		"IO ports.";
	return desc;
}

byte MSXCPUInterface::IODebug::read(unsigned address)
{
	const EmuTime& time = parent.scheduler.getCurrentTime();
	return parent.readIO((word)address, time); // TODO make peekIO() method
}

void MSXCPUInterface::IODebug::write(unsigned address, byte value)
{
	const EmuTime& time = parent.scheduler.getCurrentTime();
	parent.writeIO((word)address, value, time);
}


// class SlotMapCmd

MSXCPUInterface::SlotMapCmd::SlotMapCmd(MSXCPUInterface& parent_)
	: parent(parent_)
{
}

string MSXCPUInterface::SlotMapCmd::execute(const vector<string>& tokens)
{
	return parent.getSlotMap();
}

string MSXCPUInterface::SlotMapCmd::help(const vector<string>& tokens) const
{
	return "Prints which slots contain which devices.\n";
}


// class SlotSelectCmd

MSXCPUInterface::SlotSelectCmd::SlotSelectCmd(MSXCPUInterface& parent_)
	: parent(parent_)
{
}

string MSXCPUInterface::SlotSelectCmd::execute(const vector<string>& tokens)
{
	return parent.getSlotSelection();
}

string MSXCPUInterface::SlotSelectCmd::help(const vector<string>& tokens) const
{
	return "Prints which slots are currently selected.\n";
}


// class TurborCPUInterface 

TurborCPUInterface::TurborCPUInterface()
	: delayDevice(NULL)
{
}

TurborCPUInterface::~TurborCPUInterface()
{
	delete delayDevice;
}

void TurborCPUInterface::register_IO_In(byte port, MSXIODevice* device)
{
	if ((0x98 <= port) && (port <= 0x9B)) {
		device = getDelayDevice(device);
	}
	MSXCPUInterface::register_IO_In(port, device);
}

void TurborCPUInterface::register_IO_Out(byte port, MSXIODevice* device)
{
	if ((0x98 <= port) && (port <= 0x9B)) {
		device = getDelayDevice(device);
	}
	MSXCPUInterface::register_IO_Out(port, device);
}

MSXIODevice *TurborCPUInterface::getDelayDevice(MSXIODevice* device)
{
	if (delayDevice == NULL) {
		delayDevice = new VDPIODelay(device, EmuTime::zero);
	}
	return delayDevice;
}

} // namespace openmsx
