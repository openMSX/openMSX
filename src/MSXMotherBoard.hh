// $Id$

#ifndef __MSXMOTHERBOARD_HH__
#define __MSXMOTHERBOARD_HH__

#include <fstream>
#include <vector>
#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "EmuTime.hh"


class MSXMotherBoard : public MSXDevice
{	
	public:
		/**
		 * Constructor
		 * This is a singleton class, constructor may only be used
		 * once in the class devicefactory.
		 */
		MSXMotherBoard(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		~MSXMotherBoard();
		
		/**
		 * this class is a singleton class
		 * usage: MSXConfig::instance()->method(args);
		 */
		static MSXMotherBoard *instance();
	 
		// Devices can (un)register IO ports
		// Should be done during the InitMSX method
		void register_IO_In(byte port,MSXDevice *device);
		void register_IO_Out(byte port,MSXDevice *device);
		//To register devices in the slotlayout of the msx device
		//fe. the memmorymapper registers its childs with this method
		void addDevice(MSXDevice *device);
		void registerSlottedDevice(MSXDevice *device,int PrimSl,int SecSL,int Page);
		// To remove a device completely from configuration
		// fe. yanking a cartridge out of the msx
		void removeDevice(MSXDevice *device);

		void raiseIRQ();
		void lowerIRQ();
		bool IRQstatus();

		void InitMSX();
		void StartMSX();
		void ResetMSX();
		void StopMSX();
		void DestroyMSX();

		void RestoreMSX();
		void SaveStateMSX(std::ofstream &savestream);

		// This will be used by CPU to read data from "visual" devices
		byte readMem(word address, EmuTime &time);
		void writeMem(word address, byte value, EmuTime &time);
		byte readIO(byte port, EmuTime &time);
		void writeIO(byte port, byte value, EmuTime &time);

		void set_A8_Register(byte value);

	private:

		MSXDevice* IO_In[256];
		MSXDevice* IO_Out[256];
		std::vector<MSXDevice*> availableDevices;
		
		MSXDevice* SlotLayout[4][4][4];
		byte SubSlot_Register[4];
		byte A8_Register;
		byte PrimarySlotState[4];
		byte SecondarySlotState[4];
		bool isSubSlotted[4];
		MSXDevice* visibleDevices[4]; 
		
		int IRQLine;

		static MSXMotherBoard *oneInstance;
};
#endif //__MSXMOTHERBOARD_HH__
