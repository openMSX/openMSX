// $Id$

#include <cstdio>

#include "MSXCPUInterface.hh"
#include "DummyDevice.hh"
#include "CommandController.hh"
#include "MSXCPU.hh"
#include "MSXConfig.hh"
#include "CartridgeSlotManager.hh"


MSXCPUInterface* MSXCPUInterface::instance()
{
	static MSXCPUInterface* oneInstance = NULL;
	if (oneInstance == NULL)
		oneInstance = new MSXCPUInterface();
	return oneInstance;
}

MSXCPUInterface::MSXCPUInterface()
{
	PRT_DEBUG("Creating an MSXCPUInterface object");

	DummyDevice* dummy = DummyDevice::instance();
	for (int port=0; port<256; port++) {
		IO_In [port] = dummy;
		IO_Out[port] = dummy;
	}
	for (int primSlot=0; primSlot<4; primSlot++) {
		isSubSlotted[primSlot] = false;
		subSlotRegister[primSlot] = 0;
		for (int secSlot=0; secSlot<4; secSlot++) {
			for (int page=0; page<4; page++) {
				slotLayout[primSlot][secSlot][page] = dummy;
			}
		}
	}
	for (int page=0; page<4; page++) {
		visibleDevices[page] = 0;
	}

	Config* config = MSXConfig::instance()->
		getConfigById("MotherBoard");
	std::list<Device::Parameter*>* subslotted_list;
	subslotted_list = config->getParametersWithClass("subslotted");
	std::list<Device::Parameter*>::const_iterator it;
	for (it = subslotted_list->begin(); it != subslotted_list->end(); it++) {
		bool hasSubs = false;
		if ((*it)->value == "true") {
			hasSubs = true;
		}
		int counter = atoi((*it)->name.c_str());
		isSubSlotted[counter] = hasSubs;
		PRT_DEBUG("Slot: " << counter << " expanded: " << hasSubs);
	}
	config->getParametersWithClassClean(subslotted_list);

	// Note: SlotState is initialised at reset

	// Register console commands
	CommandController::instance()->registerCommand(&slotMapCmd,    "slotmap");
	CommandController::instance()->registerCommand(&slotSelectCmd, "slotselect");
}

MSXCPUInterface::~MSXCPUInterface()
{
	CommandController::instance()->unregisterCommand(&slotMapCmd,    "slotmap");
	CommandController::instance()->unregisterCommand(&slotSelectCmd, "slotselect");
}


void MSXCPUInterface::register_IO_In(byte port, MSXIODevice *device)
{
	if (IO_In[port] == DummyDevice::instance()) {
		PRT_DEBUG(device->getName() << " registers In-port "
		          << std::hex << (int)port << std::dec);
		IO_In[port] = device;
	} else {
		PRT_ERROR(device->getName() << " trying to register taken In-port "
		          << std::hex << (int)port << std::dec);
	}
}

void MSXCPUInterface::register_IO_Out(byte port, MSXIODevice *device)
{
	if (IO_Out[port] == DummyDevice::instance()) {
		PRT_DEBUG(device->getName() << " registers Out-port "
		          << std::hex << (int)port << std::dec);
		IO_Out[port] = device;
	} else {
		PRT_ERROR(device->getName() << " trying to register taken Out-port "
		          << std::hex << (int)port << std::dec);
	}
}

void MSXCPUInterface::registerSlot(MSXMemDevice *device,
                                   int primSl, int secSl, int page)
{
	if (!isSubSlotted[primSl] && secSl != 0) {
		std::ostringstream s;
		s << "slot " << primSl << "." << secSl
			<< " does not exist, because slot is not expanded";
		throw MSXException(s.str());
	}
	if (slotLayout[primSl][secSl][page] == DummyDevice::instance()) {
		PRT_DEBUG(device->getName() << " registers at "<<primSl<<" "<<secSl<<" "<<page);
		slotLayout[primSl][secSl][page] = device;
	} else {
		PRT_ERROR(device->getName() << " trying to register taken slot");
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
	PRT_DEBUG("debug1");
	RegPostSlot regPostSlot(device, pages);
	regPostSlots.push_back(regPostSlot);
}

void MSXCPUInterface::registerPostSlots()
{
	PRT_DEBUG("debug2");
	std::list<RegPostSlot>::iterator it;
	for (it = regPostSlots.begin(); it != regPostSlots.end(); it++) {
		int ps, ss;
		CartridgeSlotManager::instance()->getSlot(ps, ss);
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
		MSXCPU::instance()->invalidateCache(page * 0x4000,
		                        0x4000 / CPU::CACHE_LINE_SIZE);
	}
}

void MSXCPUInterface::reset()
{
	setPrimarySlots(0);
}

void MSXCPUInterface::setPrimarySlots(byte value)
{
	for (int page=0; page<4; page++, value>>=2) {
		// Change the slot structure
		primarySlotState[page] = value & 3;
		secondarySlotState[page] = (subSlotRegister[value&3] >> (page*2)) & 3;
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
			for (int page=0; page<4; page++, value>>=2) {
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
	if ((start == 0x10000-CPU::CACHE_LINE_SIZE) &&	// contains 0xffff
	    (isSubSlotted[primarySlotState[3]]))
		return NULL;
	return visibleDevices[start>>14]->getReadCacheLine(start);
}

byte* MSXCPUInterface::getWriteCacheLine(word start) const
{
	if ((start == 0x10000-CPU::CACHE_LINE_SIZE) &&	// contains 0xffff
	    (isSubSlotted[primarySlotState[3]]))
		return NULL;
	return visibleDevices[start>>14]->getWriteCacheLine(start);
}

std::string MSXCPUInterface::getSlotMap()
{
	std::ostringstream out;
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

void MSXCPUInterface::printSlotMapPages(
	std::ostream &out, MSXMemDevice *devices[])
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

std::string MSXCPUInterface::getSlotSelection()
{
	std::ostringstream out;
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


void MSXCPUInterface::SlotMapCmd::execute(const std::vector<std::string> &tokens,
                                          const EmuTime &time)
{
	print(MSXCPUInterface::instance()->getSlotMap());
}
void MSXCPUInterface::SlotMapCmd::help(const std::vector<std::string> &tokens) const
{
	print("Prints which slots contain which devices.");
}

void MSXCPUInterface::SlotSelectCmd::execute(const std::vector<std::string> &tokens,
                                             const EmuTime &time)
{
	print(MSXCPUInterface::instance()->getSlotSelection());
}
void MSXCPUInterface::SlotSelectCmd::help(const std::vector<std::string> &tokens) const
{
	print("Prints which slots are currently selected.");
}

