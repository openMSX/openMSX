#include "MSXDevice.hh"
#include "linkedlist.hh"
#include "MSXMotherBoard.hh"

MSXZ80 *MSXMotherBoard::CPU;

MSXMotherBoard::MSXMotherBoard()
{
cout << "Creating an MSXMotherBoard object \n";
availableDevices=new MSXDevList();
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
	if (IO_In[port]=0){
	  IO_In[port]=device;
	} else {
	  cerr << "Second device trying to register taken IO_In-port";
	}
}
void MSXMotherBoard::register_IO_Out(byte port,MSXDevice *device)
{
  if ( IO_Out[port]){
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
	visibleDevices[address>>14]->readMem(address,TStates);
}
void MSXMotherBoard::writeMem(word address,byte value,UINT64 TStates)
{
	visibleDevices[address>>14]->writeMem(address,value,TStates);
}
byte MSXMotherBoard::readIO(byte port,UINT64 TStates)
{
	IO_In[port]->readIO(port,TStates);
}
void MSXMotherBoard::writeIO(byte port,byte value,UINT64 TStates)
{
	IO_Out[port]->writeIO(port,value,TStates);
}

