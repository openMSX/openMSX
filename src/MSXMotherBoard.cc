// $Id$

#include "MSXDevice.hh"
#include "linkedlist.hh"
#include "MSXMotherBoard.hh"

MSXZ80 *MSXMotherBoard::CPU;

MSXMotherBoard::MSXMotherBoard()
{
cout << "Creating an MSXMotherBoard object \n";
  availableDevices=new MSXDevList();
  emptydevice=new MSXDevice();
  for (int i=0;i<256;i++){
	IO_In[i]=emptydevice;
	IO_Out[i]=emptydevice;
  }
  for (int i=0;i<4;i++){
	isSubSlotted[i]=false;
}
}

MSXMotherBoard::~MSXMotherBoard()
{
cout << "Detructing an MSXMotherBoard object \n";
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
   availableDevices->append(device);
}
void MSXMotherBoard::setActiveCPU(MSXZ80 *device)
{
	MSXMotherBoard::CPU=device;
}
void MSXMotherBoard::registerSlottedDevice(MSXDevice *device,int PrimSl,int SecSL,int Page)
{
	 cout<<"Registering device at "<<PrimSl<<" "<<SecSL<<" "<<Page<<"\n";
	 SlotLayout[PrimSl][SecSL][Page]=device;
}
//void MSXMotherBoard::raiseInterupt()
//{
//	cout<<"Interrupt raised\n";
//}

void MSXMotherBoard::ResetMSX()
{
	availableDevices->fromStart();
	do {
		availableDevices->device->reset();
	} while ( availableDevices->toNext() );
}
void MSXMotherBoard::InitMSX()
{
	availableDevices->fromStart();
	do {
		availableDevices->device->init();
	} while ( availableDevices->toNext() );

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
void MSXMotherBoard::SaveStateMSX(ofstream savestream)
{
	availableDevices->fromStart();
	do {
//TODO:		availableDevices->device->saveState(savestream);
	} while ( availableDevices->toNext() );
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

byte MSXMotherBoard::readIO(byte port,UINT64 TStates)
{
	IO_In[port]->readIO(port,TStates);
}
void MSXMotherBoard::writeIO(byte port,byte value,UINT64 TStates)
{
	IO_Out[port]->writeIO(port,value,TStates);
}

