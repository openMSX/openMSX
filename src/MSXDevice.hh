// $Id$

#ifndef __MSXDEVICE_HH__
#define __MSXDEVICE_HH__

#include <iostream.h>
#include <fstream.h>
#include <string.h>
#include "msxconfig.hh"
#include "emutime.hh"
#include "openmsx.hh"

//class MSXConfig.Device;
// is redefinition class MSXConfig { class Device;};

//class MSXConfig.Device;
// is redefinition class MSXConfig { class Device;};

class MSXDevice
{
	protected:
		//constructor and destructor
		MSXDevice(void);
	public:
		virtual ~MSXDevice(void);
		//
		//instantiate method used in DeviceFactory
		//Must be overwritten in derived classes !!!!
		//static MSXDevice* instantiate(void);
		virtual void setConfigDevice(MSXConfig::Device *config);
		
		// interaction with CPU
		virtual byte readMem(word address, Emutime &time);
		virtual void writeMem(word address, byte value, Emutime &time);
		virtual byte readIO(byte port, Emutime &time);
		virtual void writeIO(byte port, byte value, Emutime &time);
		virtual void executeUntilEmuTime(const Emutime &time);
		//virtual int executeTStates(int TStates);
		//virtual int getUsedTStates(void);
		//
		// mainlife cycle of an MSXDevice
		virtual void init();
		virtual void start();
		virtual void stop();
		virtual void reset();
		//
		virtual void saveState(ofstream &writestream);
		virtual void restoreState(string &devicestring, ifstream &readstream);
		
		/*virtual void setParameter(string &param, string &value);
		virtual const string &getParameter(string &param);
		virtual int getNrParameters();
		virtual const string &getParameterTxt(int nr);
		virtual const string &getParamShortHelp(int nr);
		virtual const string &getParamLongHelp(int nr);*/
		
		virtual const string &getName();
	protected:
		MSXConfig::Device *deviceConfig;
		//These are used for save/restoreState see note over
		//savefile-structure
		bool writeSaveStateHeader(ofstream &writestream);
		bool checkSaveStateHeader(string &devicestring);
		const string* deviceName;
		char* deviceVersion;
		// To ease the burden of keeping IRQ state
		bool isIRQset;
		void setInterrupt();
		void resetInterrupt();
};

#endif //__MSXDEVICE_H__

