// $Id$

#ifndef __MSXDEVICE_HH__
#define __MSXDEVICE_HH__

#include <iostream.h>
#include <fstream.h>
#include "msxconfig.hh"

//using namespace std ;
//using std::ofstream ;
//using std::ifstream ;

typedef unsigned char byte;         // 8 bit
typedef unsigned short word;          // 16 bit 
typedef unsigned long int UINT64;   // 64 bit

//class MSXConfig.Device;
// is redefinition class MSXConfig { class Device;};

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
		virtual void setConfigDevice(MSXConfig::Device *config);
		
		// interaction with CPU
		virtual byte readMem(word address,UINT64 TStates);
		virtual void writeMem(word address,byte value,UINT64 TStates);
		virtual byte readIO(byte port,UINT64 TStates);
		virtual void writeIO(byte port,byte value,UINT64 TStates);
		virtual void executeUntilEmuTime(UINT64 TStates);
		virtual int executeTStates(int TStates);
		virtual int getUsedTStates(void);
		//
		// mainlife cycle of an MSXDevice
		virtual void init();
		virtual void start();
		virtual void stop();
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
		MSXConfig::Device *deviceConfig;
		//These are used for save/restoreState see note over
		//savefile-structure
		bool writeSaveStateHeader(ofstream writestream);
		bool checkSaveStateHeader(char *devicestring);
		const string* deviceName;
		char* deviceVersion;
		// To ease the burden of keeping IRQ state
		bool isIRQset;
		void setInterrupt();
		void resetInterrupt();
};
#endif //__MSXDEVICE_H__

