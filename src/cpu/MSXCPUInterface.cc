// $Id$

#include "MSXCPUInterface.hh"
#include "DummyDevice.hh"
#include "ConsoleSource/Console.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"	//


MSXCPUInterface::MSXCPUInterface(MSXConfig::Config *config)
{
	PRT_DEBUG("Creating an MSXCPUInterface object");

	DummyDevice* dummy = DummyDevice::instance();
	for (int port=0; port<256; port++) {
		IO_In [port] = dummy;
		IO_Out[port] = dummy;
	}
	for (int primSlot=0; primSlot<4; primSlot++) {
		isSubSlotted[primSlot] = false;
		SubSlot_Register[primSlot] = 0;
		for (int secSlot=0; secSlot<4; secSlot++) {
			for (int page=0; page<4; page++) {
				SlotLayout[primSlot][secSlot][page]=dummy;
			}
		}
	}
	for (int page=0; page<4; page++) {
		visibleDevices[page] = 0;
	}

	std::list<MSXConfig::Device::Parameter*>* subslotted_list;
	subslotted_list = config->getParametersWithClass("subslotted");
	std::list<MSXConfig::Device::Parameter*>::const_iterator i;
	for (i=subslotted_list->begin(); i != subslotted_list->end(); i++) {
		bool hasSubs=false;
		if ((*i)->value == "true") {
			hasSubs=true;
		}
		int counter=atoi((*i)->name.c_str());
		isSubSlotted[counter]=hasSubs;
		PRT_DEBUG("Slot: " << counter << " expanded: " << hasSubs);
	}
	config->getParametersWithClassClean(subslotted_list);

	// Note: PrimarySlotState and SecondarySlotState will be
	//       initialised at reset.

	// Register console commands.
	Console::instance()->registerCommand(slotMapCmd, "slotmap");
	Console::instance()->registerCommand(slotSelectCmd, "slotselect");
}

void MSXCPUInterface::register_IO_In(byte port, MSXIODevice *device)
{
	if (IO_In[port] == DummyDevice::instance()) {
		PRT_DEBUG (device->getName() << " registers In-port " <<std::hex<< (int)port<<std::dec);
		IO_In[port] = device;
	} else {
		PRT_ERROR (device->getName() << " trying to register taken In-port "
		                        <<std::hex << (int)port << std::dec);
	}
}

void MSXCPUInterface::register_IO_Out(byte port, MSXIODevice *device)
{
	if (IO_Out[port] == DummyDevice::instance()) {
		PRT_DEBUG (device->getName() << " registers Out-port " <<std::hex<<(int)port<<std::dec);
		IO_Out[port] = device;
	} else {
		PRT_ERROR (device->getName() << " trying to register taken Out-port "
		                        <<std::hex<< (int)port<<std::dec);
	}
}

void MSXCPUInterface::registerSlottedDevice(MSXMemDevice *device, int primSl, int secSl, int page)
{
	if (!isSubSlotted[primSl] && secSl != 0) {
		std::ostringstream s;
		s << "slot " << primSl << "." << secSl
			<< " does not exist, because slot is not expanded";
		throw MSXException(s.str());
	}
	if (SlotLayout[primSl][secSl][page] == DummyDevice::instance()) {
		PRT_DEBUG(device->getName() << " registers at "<<primSl<<" "<<secSl<<" "<<page);
		SlotLayout[primSl][secSl][page] = device;
	} else {
		PRT_ERROR(device->getName() << " trying to register taken slot");
	}
}

void MSXCPUInterface::updateVisible(int page)
{
	MSXMemDevice *newDevice = SlotLayout [PrimarySlotState[page]]
	                                     [SecondarySlotState[page]]
	                                     [page];
	if (visibleDevices[page] != newDevice) {
		visibleDevices[page] = newDevice;
		// Different device, so cache is no longer valid.
		MSXCPU::instance()->invalidateCache(
			page*0x4000, 0x4000/CPU::CACHE_LINE_SIZE);
	}
}

void MSXCPUInterface::setPrimarySlots(byte value)
{
	for (int page=0; page<4; page++, value>>=2) {
		// Change the slot structure
		PrimarySlotState[page] = value&3;
		SecondarySlotState[page] = 3&(SubSlot_Register[value&3]>>(page*2));
		// Change the visible devices
		updateVisible(page);
	}
}

byte MSXCPUInterface::readMem(word address, const EmuTime &time)
{
	if (address == 0xFFFF) {
		int CurrentSSRegister = PrimarySlotState[3];
		if (isSubSlotted[CurrentSSRegister]) {
			return 255^SubSlot_Register[CurrentSSRegister];
		}
	}
	return visibleDevices[address>>14]->readMem(address, time);
}

void MSXCPUInterface::writeMem(word address, byte value, const EmuTime &time)
{
	if (address == 0xFFFF) {
		int CurrentSSRegister = PrimarySlotState[3];
		if (isSubSlotted[CurrentSSRegister]) {
			SubSlot_Register[CurrentSSRegister] = value;
			for (int page=0; page<4; page++, value>>=2) {
				if (CurrentSSRegister == PrimarySlotState[page]) {
					SecondarySlotState[page] = value&3;
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


bool MSXCPUInterface::IRQStatus()
{
	return (bool)IRQLine;
}
void MSXCPUInterface::raiseIRQ()
{
	IRQLine++;
}
void MSXCPUInterface::lowerIRQ()
{
	assert (IRQLine != 0);
	IRQLine--;
}
void MSXCPUInterface::resetIRQLine()
{
	IRQLine = 0;
}
int MSXCPUInterface::IRQLine = 0;

byte* MSXCPUInterface::getReadCacheLine(word start)
{
	if ((start == 0x10000-CPU::CACHE_LINE_SIZE) &&	// contains 0xffff
	    (isSubSlotted[PrimarySlotState[3]]))
		return NULL;
	return visibleDevices[start>>14]->getReadCacheLine(start);
}

byte* MSXCPUInterface::getWriteCacheLine(word start)
{
	if ((start == 0x10000-CPU::CACHE_LINE_SIZE) &&	// contains 0xffff
	    (isSubSlotted[PrimarySlotState[3]]))
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
				printSlotMapPages(out, SlotLayout[prim][sec]);
			}
		} else {
			out << "slot " << prim << ":\n";
			printSlotMapPages(out, SlotLayout[prim][0]);
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

		int prim = PrimarySlotState[page];
		int sec = (SubSlot_Register[prim] >> (page * 2)) & 3;
		if (isSubSlotted[prim]) {
			out << "slot " << prim << "." << sec << "\n";
		} else {
			out << "slot " << prim << "\n";
		}
	}
	return out.str();
}


void MSXCPUInterface::SlotMapCmd::execute(const std::vector<std::string> &tokens)
{
	Console::instance()->print(MSXMotherBoard::instance()->getSlotMap());
}
void MSXCPUInterface::SlotMapCmd::help   (const std::vector<std::string> &tokens)
{
	Console::instance()->print("Prints which slots contain which devices.");
}

void MSXCPUInterface::SlotSelectCmd::execute(const std::vector<std::string> &tokens)
{
	Console::instance()->print(MSXMotherBoard::instance()->getSlotSelection());
}
void MSXCPUInterface::SlotSelectCmd::help   (const std::vector<std::string> &tokens)
{
	Console::instance()->print("Prints which slots are currently selected.");
}

