// $Id$

#include "MSXMotherBoard.hh"

MSXZ80 *MSXMotherBoard::CPU;

MSXMotherBoard::MSXMotherBoard()
{
	PRT_DEBUG("Creating an MSXMotherBoard object \n");
	availableDevices = new vector<MSXDevice*>();
	emptydevice = new MSXDevice();
	for (int i=0; i<256; i++) {
		IO_In[i]  = emptydevice;
		IO_Out[i] = emptydevice;
	}
	for (int i=0; i<4; i++) {
		isSubSlotted[i] = false;
	}
}

MSXMotherBoard::~MSXMotherBoard()
{
	PRT_DEBUG("Detructing an MSXMotherBoard object \n");
	delete availableDevices;
}

MSXMotherBoard *volatile MSXMotherBoard::oneInstance;
 
MSXMotherBoard *MSXMotherBoard::instance()
{
	if (oneInstance == 0)
		oneInstance = new MSXMotherBoard;
	return oneInstance;
}

void MSXMotherBoard::register_IO_In(byte port,MSXDevice *device)
{
	assert (IO_In[port]==emptydevice);
	IO_In[port]=device;
}
void MSXMotherBoard::register_IO_Out(byte port,MSXDevice *device)
{
	assert (IO_Out[port]==emptydevice);
	IO_Out[port]=device;
}
void MSXMotherBoard::addDevice(MSXDevice *device)
{
	availableDevices->push_back(device);
}
void MSXMotherBoard::setActiveCPU(MSXZ80 *device)
{
	MSXMotherBoard::CPU=device;
}
void MSXMotherBoard::registerSlottedDevice(MSXDevice *device,int PrimSl,int SecSL,int Page)
{
	 PRT_DEBUG("Registering device at "<<PrimSl<<" "<<SecSL<<" "<<Page<<"\n");
	 SlotLayout[PrimSl][SecSL][Page]=device;
}
//void MSXMotherBoard::raiseInterupt()
//{
//	PRT_DEBUG("Interrupt raised\n");
//}

void MSXMotherBoard::ResetMSX()
{
	vector<MSXDevice*>::iterator i;
	for (i = availableDevices->begin(); i != availableDevices->end(); i++) {
		(*i)->reset();
	}
}
void MSXMotherBoard::InitMSX()
{
	vector<MSXDevice*>::iterator i;
	for (i = availableDevices->begin(); i != availableDevices->end(); i++) {
		(*i)->init();
	}
}
void MSXMotherBoard::StartMSX()
{
	//TODO this should be done by the PPI
	visibleDevices[0]=SlotLayout[0][0][0];
	visibleDevices[1]=SlotLayout[0][0][1];
	visibleDevices[2]=SlotLayout[0][0][2];
	visibleDevices[3]=SlotLayout[0][0][3];
	scheduler.scheduleEmulation();
}
void MSXMotherBoard::SaveStateMSX(ofstream &savestream)
{
	vector<MSXDevice*>::iterator i;
	for (i = availableDevices->begin(); i != availableDevices->end(); i++) {
	//TODO	(*i)->saveState(savestream);
	}
}

byte MSXMotherBoard::readMem(word address, Emutime &time)
{
	int CurrentSSRegister;
	if (address == 0xFFFF){
		CurrentSSRegister=(A8_Register>>6)&3;
		if (isSubSlotted[CurrentSSRegister]){	
			return 255^SubSlot_Register[CurrentSSRegister];
			}
		}
		//visibleDevices[address>>14]->readMem(address,TStates);
	return visibleDevices[address>>14]->readMem(address, time);
	
}
void MSXMotherBoard::writeMem(word address, byte value, Emutime &time)
{
	int CurrentSSRegister;
	if (address == 0xFFFF){
		// TODO: write to correct subslotregister
		CurrentSSRegister=(A8_Register>>6)&3;
		if (isSubSlotted[CurrentSSRegister]){	
			SubSlot_Register[CurrentSSRegister]=value;
			//TODO :do actual switching
			for (int i=0;i<4;i++,value>>=2){
				if (CurrentSSRegister ==  PrimarySlotState[i]){
					SecondarySlotState[i]=value&3;
					// Change the visible devices
					visibleDevices[i]= SlotLayout 
						[PrimarySlotState[i]]
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

void MSXMotherBoard::set_A8_Register(byte value)
{
	for (int J=0; J<4; J++, value>>=2) {
		// Change the slot structure
		PrimarySlotState[J]=value&3;
		SecondarySlotState[J]=3&(SubSlot_Register[value&3]>>(J*2));
		// Change the visible devices
		visibleDevices[J]= SlotLayout 
			[PrimarySlotState[J]]
			[SecondarySlotState[J]]
			[J];
	}
}

byte MSXMotherBoard::readIO(byte port, Emutime &time)
{
	return IO_In[port]->readIO(port, time);
}
void MSXMotherBoard::writeIO(byte port, byte value, Emutime &time)
{
	IO_Out[port]->writeIO(port, value, time);
}

