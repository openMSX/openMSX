// $Id$

// 15-08-2001: add start-call loop
// 31-08-2001: added dummy devices in all empty slots during instantiate
// 01-09-2001: Fixed set_a8_register
//#include "MSXDevice.hh"
//#include "linkedlist.hh"
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
	for (int i=0;i<4;i++){
	  for (int j=0;j<4;j++){
	    for (int k=0;k<4;k++){
	      SlotLayout[i][j][k]=emptydevice;
	    }
	  }
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
	if (IO_In[port] == emptydevice){
	  IO_In[port]=device;
	} else {
	  cerr << "Second device trying to register taken IO_In-port";
	}
}
void MSXMotherBoard::register_IO_Out(byte port,MSXDevice *device)
{
  if ( IO_Out[port] == emptydevice){
    IO_Out[port]=device;
  } else {
    cerr << "Second device trying to register taken IO_Out-port";
  }
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
    bool hasSubs;
    int counter;
	// Make sure that the MotherBoard is correctly 'init'ed.
	list<const MSXConfig::Device::Parameter*> subslotted_list = deviceConfig->getParametersWithClass("subslotted");
	for (list<const MSXConfig::Device::Parameter*>::const_iterator i=subslotted_list.begin(); i != subslotted_list.end(); i++)
	{
      hasSubs=false;
      if ((*i)->value.compare("true") == 0){
        hasSubs=true;
      }
      counter=atoi((*i)->name.c_str());
      isSubSlotted[counter]=hasSubs;
     
		cout << "Parameter, name: " << (*i)->name;
		cout << " value: " << (*i)->value;
		cout << " class: " << (*i)->clasz << endl;
	}
//	availableDevices->fromStart();
//	do {
//		availableDevices->device->init();
//	} while ( availableDevices->toNext() );

	vector<MSXDevice*>::iterator i;
	for (i = availableDevices->begin(); i != availableDevices->end(); i++) {
		(*i)->init();
	}
}
void MSXMotherBoard::StartMSX()
{
/*
void MSXMotherBoard::insertStamp(UINT64 timestamp,MSXDevice *activedevice)
{
	scheduler.insertStamp(timestamp,activedevice);
}
void MSXMotherBoard::setLaterSP(UINT64 latertimestamp,MSXDevice *activedevice)
{
	scheduler.setLaterSP(latertimestamp,activedevice);

*/
	//TODO this should be done by the PPI
	visibleDevices[0]=SlotLayout[0][0][0];
	visibleDevices[1]=SlotLayout[0][0][1];
	visibleDevices[2]=SlotLayout[0][0][2];
	visibleDevices[3]=SlotLayout[0][0][3];
	vector<MSXDevice*>::iterator i;
	for (i = availableDevices->begin(); i != availableDevices->end(); i++) {
		(*i)->start();
	}
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

byte MSXMotherBoard::readMem(word address,UINT64 TStates)
{
	int CurrentSSRegister;
	if (address == 0xFFFF){
		CurrentSSRegister=(A8_Register>>6)&3;
		if (isSubSlotted[CurrentSSRegister]){	
			return 255^SubSlot_Register[CurrentSSRegister];
			}
		}
		//visibleDevices[address>>14]->readMem(address,TStates);
	return visibleDevices[address>>14]->readMem(address,TStates);
	
}
void MSXMotherBoard::writeMem(word address,byte value,UINT64 TStates)
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
	visibleDevices[address>>14]->writeMem(address,value,TStates);
	
}

void MSXMotherBoard::set_A8_Register(byte value)
{
	A8_Register=value;
	for (int J=0;J<4;J++,value>>=2){
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

