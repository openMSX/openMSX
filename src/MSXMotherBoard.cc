// $Id$

// 15-08-2001: add start-call loop
// 31-08-2001: added dummy devices in all empty slots during instantiate
// 01-09-2001: Fixed set_a8_register

#include "MSXMotherBoard.hh"
#include "RealTime.hh"
#include "DummyDevice.hh"
#include "Leds.hh"
#include "MSXDevice.hh"
#include "MSXIODevice.hh"
#include "MSXMemDevice.hh"

MSXMotherBoard::MSXMotherBoard()
{
	PRT_DEBUG("Creating an MSXMotherBoard object");
	
	for (int i=0; i<256; i++) {
		IO_In[i]  = DummyDevice::instance();
		IO_Out[i] = DummyDevice::instance();
	}
	for (int i=0; i<4; i++) {
		isSubSlotted[i] = false;
	}
	for (int i=0;i<4;i++) {
		for (int j=0;j<4;j++) {
			for (int k=0;k<4;k++) {
				SlotLayout[i][j][k]=DummyDevice::instance();
			}
		}
	}

	config = MSXConfig::instance()->getConfigById("MotherBoard");
	std::list<const MSXConfig::Device::Parameter*> subslotted_list;
	subslotted_list = config->getParametersWithClass("subslotted");
	std::list<const MSXConfig::Device::Parameter*>::const_iterator i;
	for (i=subslotted_list.begin(); i != subslotted_list.end(); i++) {
		bool hasSubs=false;
		if ((*i)->value == "true") {
			hasSubs=true;
		}
		int counter=atoi((*i)->name.c_str());
		isSubSlotted[counter]=hasSubs;
		PRT_DEBUG("Slot: " << counter << " expanded: " << hasSubs);
	}
}

MSXMotherBoard::~MSXMotherBoard()
{
	PRT_DEBUG("Destructing an MSXMotherBoard object");
}

MSXMotherBoard *MSXMotherBoard::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new MSXMotherBoard();
	}
	return oneInstance;
}
MSXMotherBoard *MSXMotherBoard::oneInstance = NULL;


void MSXMotherBoard::register_IO_In(byte port, MSXIODevice *device)
{
	if (IO_In[port] == DummyDevice::instance()) {
		PRT_DEBUG (device->getName() << " registers In-port " << (int)port);
		IO_In[port] = device;
	} else {
		PRT_ERROR (device->getName() << " trying to register taken In-port " 
		                        << (int)port);
	}
}

void MSXMotherBoard::register_IO_Out(byte port, MSXIODevice *device)
{
	if ( IO_Out[port] == DummyDevice::instance()) {
		PRT_DEBUG (device->getName() << " registers Out-port " << (int)port);
		IO_Out[port] = device;
	} else {
		PRT_ERROR (device->getName() << " trying to register taken Out-port "
		                        << (int)port);
	}
}

void MSXMotherBoard::addDevice(MSXDevice *device)
{
	availableDevices.push_back(device);
}

void MSXMotherBoard::registerSlottedDevice(MSXMemDevice *device, int primSl, int secSl, int page)
{
	if (SlotLayout[primSl][secSl][page] == DummyDevice::instance()) {
		PRT_DEBUG(device->getName() << " registers at "<<primSl<<" "<<secSl<<" "<<page);
		SlotLayout[primSl][secSl][page] = device;
	} else {
		PRT_ERROR(device->getName() << " trying to register taken slot");
	}
}

void MSXMotherBoard::ResetMSX(const EmuTime &time)
{
	IRQLine = 0;
	set_A8_Register(0);
	std::vector<MSXDevice*>::iterator i;
	for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
		(*i)->reset(time);
	}
}

void MSXMotherBoard::StartMSX()
{
	IRQLine = 0;
	set_A8_Register(0);
	Leds::instance()->setLed(Leds::POWER_ON);
	RealTime::instance();
	Scheduler::instance()->scheduleEmulation();
}

void MSXMotherBoard::DestroyMSX()
{
	std::vector<MSXDevice*>::iterator i;
	for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
		delete (*i);
	}
}

void MSXMotherBoard::SaveStateMSX(std::ofstream &savestream)
{
	std::vector<MSXDevice*>::iterator i;
	for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
		(*i)->saveState(savestream);
	}
}


void MSXMotherBoard::set_A8_Register(byte value)
{
	A8_Register = value;
	for (int j=0; j<=3; j++, value>>=2) {
		// Change the slot structure
		PrimarySlotState[j] = value&3;
		SecondarySlotState[j] = 3&(SubSlot_Register[value&3]>>(j*2));
		// Change the visible devices
		visibleDevices[j] = SlotLayout [PrimarySlotState[j]]
		                               [SecondarySlotState[j]]
		                               [j];
	}
}


// CPU Interface //

byte MSXMotherBoard::readMem(word address, const EmuTime &time)
{
	if (address == 0xFFFF) {
		int CurrentSSRegister = (A8_Register>>6)&3;
		if (isSubSlotted[CurrentSSRegister]) {
			return 255^SubSlot_Register[CurrentSSRegister];
		}
	}
	return visibleDevices[address>>14]->readMem(address, time);
}

void MSXMotherBoard::writeMem(word address, byte value, const EmuTime &time)
{
	if (address == 0xFFFF) {
		int CurrentSSRegister = (A8_Register>>6)&3;
		if (isSubSlotted[CurrentSSRegister]) {
			SubSlot_Register[CurrentSSRegister] = value;
			for (int i=0; i<4; i++, value>>=2) {
				if (CurrentSSRegister == PrimarySlotState[i]) {
					SecondarySlotState[i] = value&3;
					// Change the visible devices
					visibleDevices[i]= SlotLayout [PrimarySlotState[i]]
					                              [SecondarySlotState[i]]
					                              [i];
				}
			}
			return;
		}
	}
	// address is not FFFF or it is but there is no subslotregister visible
	visibleDevices[address>>14]->writeMem(address, value, time);
}

byte MSXMotherBoard::readIO(word prt, const EmuTime &time)
{
	byte port = (byte)prt;
	return IO_In[port]->readIO(port, time);
}

void MSXMotherBoard::writeIO(word prt, byte value, const EmuTime &time)
{
	byte port = (byte)prt;
	IO_Out[port]->writeIO(port, value, time);
}


bool MSXMotherBoard::IRQStatus()
{
	return (bool)IRQLine;
}

void MSXMotherBoard::raiseIRQ()
{
	IRQLine++;
}

void MSXMotherBoard::lowerIRQ()
{
	assert (IRQLine != 0);
	IRQLine--;
}

