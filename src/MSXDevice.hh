// $Id$

#ifndef __MSXDEVICE_H__
#define __MSXDEVICE_H__

#include <iostream.h>
#include <fstream.h>
//using namespace std ;
//using std::ofstream ;
//using std::ifstream ;

typedef unsigned char byte;         // 8 bit
typedef unsigned short word;          // 16 bit 
typedef unsigned long int UINT64;   // 64 bit

class MSXDevice
{
	public:
		//destructor and 
		MSXDevice(void);
		virtual ~MSXDevice(void);
		//
		//instantiate method used in DeviceFactory
		//Must be overwritten in derived classes !!!!
		static MSXDevice* instantiate(void);
		
		// interaction with CPU
		virtual byte readMem(word address,UINT64 TStates);
		virtual void writeMem(word address,byte value,UINT64 TStates);
		virtual byte readIO(byte port,UINT64 TStates);
		virtual void writeIO(byte port,byte value,UINT64 TStates);
		virtual void executeUntilEmuTime(UINT64 TStates);
		virtual int executeTStates(int TStates);
		virtual int getUsedTStates(void);
		//
		virtual void init();	// See remark 1
		virtual void reset();
		//
		virtual void saveState(ofstream writestream);
		virtual void restoreState(char *devicestring,ifstream readstream);
		virtual void setParameter(char *param,char *valuelist);
		virtual char* getParameter(char *param);
		virtual int getNrParameters();
		virtual char* getParameterTxt(int nr);
		virtual char* getParamShortHelp(int nr);
		virtual char* getParamLongHelp(int nr);
	protected:
		//These are used for save/restoreState see note over
		//savefile-structure
		bool writeSaveStateHeader(ofstream writestream);
		bool checkSaveStateHeader(char *devicestring);
		char* deviceName;
		char* deviceVersion;
		// To ease the burden of keeping IRQ state
		bool isIRQset;
		void setInterrupt();
		void resetInterrupt();
};
#endif //__MSXDEVICE_H__

/*
Remark 1 : creating of MSXDevices
---------------------------------
the sequence in which MSXDevices are used is as follows
-The main loop generates an MSXMotherBoard object and passes a configuration file.
-The motherboard parses the XML-text and if it encounters a <device>-block the following happens
--An object of the indicated class is created and added to the deviceslist
--if <slotted> are embeded in the <device>-block this device will be visible in the slotstructure of the emulated MSX.Multiple <slotted>blocks are possible f.e. a simple 64KB RAM expansion is visible in one slot,one subslot and the 4 pages of it.
--the< parameter> block is parsed and every key,value pair is passed to the setParameter methode. This method is used to set internal parameters of the MSXDevice, f.e. the amount of vram could be passed to a VDPdevice, or the number of mapper pages for a MemMapper.
-once athe configuration file is parsed and all devices created, all devices in the deviceslist have their init() method called. In the init() method the devices can allocate desired memmory, register I/O ports using the register_IO_Ini/IO_Out method of the MSXMotherBoard.
-at last the virtual MSX is started, the motherboard sends a reeset signal to all devices and starts emulating, if the user requests a reset, the reset method is called for all devices and the MSX comenses al over. The reset method is used for warm-boot, so for example memory should NOT be cleared from most memory devices.



*/
