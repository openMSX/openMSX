// $Id$

#include <cstdio>

#include "MSXCPUInterface.hh"
#include "DummyDevice.hh"
#include "CommandController.hh"
#include "MSXCPU.hh"
#include "MSXConfig.hh"
#include "Config.hh"
#include "CartridgeSlotManager.hh"
#include "VDPIODelay.hh"


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
	: slotMapCmd(*this),
	  slotSelectCmd(*this),
	  dummyDevice(DummyDevice::instance()),
	  msxConfig(MSXConfig::instance()),
	  commandController(CommandController::instance()),
	  msxcpu(MSXCPU::instance()),
	  slotManager(CartridgeSlotManager::instance())
{
	for (int port = 0; port < 256; port++) {
		IO_In [port] = &dummyDevice;
		IO_Out[port] = &dummyDevice;
	}
	for (int primSlot = 0; primSlot < 4; primSlot++) {
		primarySlotState[primSlot]=0;
		isSubSlotted[primSlot] = false;
		subSlotRegister[primSlot] = 0;
		for (int secSlot = 0; secSlot < 4; secSlot++) {
			for (int page = 0; page < 4; page++) {
				slotLayout[primSlot][secSlot][page] = &dummyDevice;
			}
		}
	}
	for (int page = 0; page < 4; page++) {
		visibleDevices[page] = 0;
	}

	Config* config = msxConfig.getConfigById("MotherBoard");
	list<Config::Parameter*>* subslotted_list;
	subslotted_list = config->getParametersWithClass("subslotted");
	for (list<Config::Parameter*>::const_iterator it = subslotted_list->begin();
	     it != subslotted_list->end(); ++it) {
		bool hasSubs = false;
		if ((*it)->value == "true") {
			hasSubs = true;
		}
		int counter = atoi((*it)->name.c_str());
		isSubSlotted[counter] = hasSubs;
	}
	config->getParametersWithClassClean(subslotted_list);

	// Note: SlotState is initialised at reset

	// Register console commands
	commandController.registerCommand(&slotMapCmd,    "slotmap");
	commandController.registerCommand(&slotSelectCmd, "slotselect");

	msxcpu.setInterface(this);
}

MSXCPUInterface::~MSXCPUInterface()
{
	msxcpu.setInterface(NULL);

	commandController.unregisterCommand(&slotMapCmd,    "slotmap");
	commandController.unregisterCommand(&slotSelectCmd, "slotselect");
}


void MSXCPUInterface::register_IO_In(byte port, MSXIODevice *device)
{
	if (IO_In[port] == &dummyDevice) {
		PRT_DEBUG(device->getName() << " registers In-port "
		          << hex << (int)port << dec);
		IO_In[port] = device;
	} else {
		ostringstream out;
		out << "Device: \"" << device->getName()
		    << "\" trying to register taken IN-port "
		    << hex << (int)port;
		throw FatalError(out.str());
	}
}

void MSXCPUInterface::register_IO_Out(byte port, MSXIODevice *device)
{
	if (IO_Out[port] == &dummyDevice) {
		PRT_DEBUG(device->getName() << " registers Out-port "
		          << hex << (int)port << dec);
		IO_Out[port] = device;
	} else {
		ostringstream out;
		out << "Device: \"" << device->getName()
		    << "\" trying to register taken OUT-port "
		    << hex << (int)port;
		throw FatalError(out.str());
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
}

void MSXCPUInterface::reset()
{
	setPrimarySlots(0);
}

void MSXCPUInterface::setPrimarySlots(byte value)
{
	for (int page = 0; page < 4; page++, value >>= 2) {
		// Change the slot structure
		primarySlotState[page] = value & 3;
		secondarySlotState[page] = (subSlotRegister[value&3] >>
		                            (page*2)) & 3;
		// Change the visible devices
		updateVisible(page);
	}
}

byte MSXCPUInterface::readMem(word address, const EmuTime &time)
{
	if (address == 0xFFFF) {
		int currentSSRegister = primarySlotState[3];
		if (isSubSlotted[currentSSRegister]) {
			return 255 ^ subSlotRegister[currentSSRegister];
		}
	}
	return visibleDevices[address>>14]->readMem(address, time);
}

void MSXCPUInterface::writeMem(word address, byte value, const EmuTime &time)
{
	if (address == 0xFFFF) {
		int currentSSRegister = primarySlotState[3];
		if (isSubSlotted[currentSSRegister]) {
			subSlotRegister[currentSSRegister] = value;
			for (int page = 0; page < 4; page++, value >>= 2) {
				if (currentSSRegister == primarySlotState[page]) {
					secondarySlotState[page] = value & 3;
					// Change the visible devices
					updateVisible(page);
				}
			}
			return;
		}
	}
	// address is not FFFF or it is but there is no subslotregister visible
	visibleDevices[address>>14]->writeMem(address, value, time);
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
	if (address == 0xFFFF) {
		int currentSSRegister = primarySlotState[3];
		if (isSubSlotted[currentSSRegister]) {
			return 255 ^ subSlotRegister[currentSSRegister];
		}
	}
	return visibleDevices[address >> 14]->peekMem(address);
}

byte MSXCPUInterface::peekMemBySlot(unsigned int address, int slot, int subslot, bool direct)
{
	if (direct){
		// TODO direct reading of the memorymapped
		// requires adapting all MSXMemDevice classes.
		return 0;
	}
	else{
	return slotLayout[slot][subslot][(address & 0xffff) >> 14]->peekMem(address & 0xffff);
	}
}

string MSXCPUInterface::getSlotMap()
{
	ostringstream out;
	for (int prim = 0; prim < 4; prim++) {
		if (isSubSlotted[prim]) {
			for (int sec = 0; sec < 4; sec++) {
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

void MSXCPUInterface::printSlotMapPages(ostream &out, MSXMemDevice *devices[])
{
	for (int page = 0; page < 4; page++) {
		char hexStr[5];
#ifndef __WIN32__
		snprintf(hexStr, sizeof(hexStr), "%04X", page * 0x4000);
#else
		sprintf(hexStr, "%04X", page * 0x4000);
#endif
		out << hexStr << ": " << devices[page]->getName() << "\n";
	}
}

string MSXCPUInterface::getSlotSelection()
{
	ostringstream out;
	for (int page = 0; page < 4; page++) {
		char pageStr[5];
#ifndef __WIN32__
		snprintf(pageStr, sizeof(pageStr), "%04X", page * 0x4000);
#else
		sprintf(pageStr, "%04X", page * 0x4000);
#endif
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
	for (int page = 0; page < 4; page++) {
		slots->primary[page] = primarySlotState[page];
		slots->secondary[page] = (subSlotRegister[slots->primary[page]] >>
		                         (page * 2)) & 3;
		slots->isSubSlotted[page] = isSubSlotted[slots->primary[page]];
	}
	return slots;
}


// class SlotMapCmd

MSXCPUInterface::SlotMapCmd::SlotMapCmd(MSXCPUInterface& parent_)
	: parent(parent_)
{
}

string MSXCPUInterface::SlotMapCmd::execute(const vector<string> &tokens)
	throw()
{
	return parent.getSlotMap();
}

string MSXCPUInterface::SlotMapCmd::help(const vector<string> &tokens) const
	throw()
{
	return "Prints which slots contain which devices.\n";
}


// class SlotSelectCmd

MSXCPUInterface::SlotSelectCmd::SlotSelectCmd(MSXCPUInterface& parent_)
	: parent(parent_)
{
}

string MSXCPUInterface::SlotSelectCmd::execute(const vector<string> &tokens)
	throw()
{
	return parent.getSlotSelection();
}

string MSXCPUInterface::SlotSelectCmd::help(const vector<string> &tokens) const
	throw()
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

void TurborCPUInterface::register_IO_In(byte port, MSXIODevice *device)
{
	if ((0x98 <= port) && (port <= 0x9B)) {
		device = getDelayDevice(device);
	}
	MSXCPUInterface::register_IO_In(port, device);
}

void TurborCPUInterface::register_IO_Out(byte port, MSXIODevice *device)
{
	if ((0x98 <= port) && (port <= 0x9B)) {
		device = getDelayDevice(device);
	}
	MSXCPUInterface::register_IO_Out(port, device);
}

MSXIODevice *TurborCPUInterface::getDelayDevice(MSXIODevice *device)
{
	if (delayDevice == NULL) {
		delayDevice = new VDPIODelay(device, EmuTime::zero);
	}
	return delayDevice;
}

} // namespace openmsx
