// $Id$

#ifndef __MSXMOTHERBOARD_HH__
#define __MSXMOTHERBOARD_HH__

#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "fstream.h"
#include "iostream.h"
#include "emutime.hh"
#include <stl.h>

class MSXZ80;
class MSXPPI;

class MSXMotherBoard : public MSXDevice
{	
	private:
		MSXMotherBoard();

		MSXDevice* IO_In[256];
		MSXDevice* IO_Out[256];
		MSXDevice* emptydevice;
		vector<MSXDevice*> *availableDevices;
		
		MSXDevice* SlotLayout[4][4][4];
		byte A8_Register;
		byte SubSlot_Register[4];
		bool isSubSlotted[4];

		static MSXMotherBoard *volatile oneInstance;

	public:
		// this class is a singleton class
		// usage: MSXConfig::instance()->method(args);
		static MSXMotherBoard *instance();
	 
		MSXDevice* visibleDevices[4]; // TODO: must be private but for test purposes, must be adjusted when #A8,subslot or mapper is changed
		Scheduler scheduler; // TODO: must be private but for test purposes
		void insertStamp(UINT64 timestamp,MSXDevice *activedevice);
		void setLaterSP(UINT64 latertimestamp,MSXDevice *activedevice);
		//constructor and destructor
		~MSXMotherBoard();
		//note: Motherboard should be a special case (singleton)
		// But since during normal use we already know this, it isn't
		// important to implement for the moment.
		//static MSXDevice* instantiate();
		
		static MSXZ80 *CPU; // for scheduler, this is the main running CPU
		
		byte PrimarySlotState[4];
		byte SecondarySlotState[4];
		// Devices can (un)register IO ports
		// Should be done during the InitMSX method
		void register_IO_In(byte port,MSXDevice *device);
		void register_IO_Out(byte port,MSXDevice *device);
		//To register devices in the slotlayout of the msx device
		//fe. the memmorymapper registers its childs with this method
		void addDevice(MSXDevice *device);
		void setActiveCPU(MSXZ80 *device);
		void registerSlottedDevice(MSXDevice *device,int PrimSl,int SecSL,int Page);

		//Once working some devices should be able to generate IRQ's
		//fe. an RS232 recievies data and wants to pass this in the 
		//RS232 of the openMSX. This kind of IRQ is external to the 
		//emulate MSX and is now emulated by the schedulling algorithm
		// in Scheduler: void raiseInterupt();
		
		// To remove a device completely from configuration
		// fe. yanking a cartridge out of the msx
		void removeDevice(MSXDevice *device);
		
		void InitMSX();
		void ResetMSX();
		void StopMSX();
		void StartMSX();
		void RestoreMSX();
		void SaveStateMSX(ofstream &savestream);
		//
		// This will be used by CPU to read data from "visual" devices
		byte readMem(word address, Emutime &time);
		void writeMem(word address, byte value, Emutime &time);
		byte readIO(byte port, Emutime &time);
		void writeIO(byte port, byte value, Emutime &time);
		void set_A8_Register(byte value);

		friend class MSXPPI;	// A8
};
#endif //__MSXMOTHERBOARD_H__
