// $Id$

#ifndef __MSXDEVICE_HH__
#define __MSXDEVICE_HH__

#include <iostream.h>
#include <fstream.h>
#include <string.h>
#include "msxconfig.hh"
#include "emutime.hh"
#include "openmsx.hh"

class MSXDevice
{
	public:
		virtual ~MSXDevice(void);
		virtual void setConfigDevice(MSXConfig::Device *config);
		
		// interaction with CPU
		virtual byte readMem(word address, Emutime &time);
		virtual void writeMem(word address, byte value, Emutime &time);
		virtual byte readIO(byte port, Emutime &time);
		virtual void writeIO(byte port, byte value, Emutime &time);
		virtual void executeUntilEmuTime(const Emutime &time);
		
		// mainlife cycle of an MSXDevice
		virtual void init();
		virtual void start();
		virtual void stop();
		virtual void reset();
		
		virtual void saveState(ofstream &writestream);
		virtual void restoreState(string &devicestring, ifstream &readstream);
		
		virtual const string &getName();

	protected:
		MSXDevice(void);
		
		MSXConfig::Device *deviceConfig;
		//These are used for save/restoreState see note over
		//savefile-structure
		bool writeSaveStateHeader(ofstream &writestream);
		bool checkSaveStateHeader(string &devicestring);
		const string* deviceName;
		char* deviceVersion;
		
		// To ease the burden of keeping IRQ state
		void setInterrupt();
		void resetInterrupt();
		bool isIRQset;
};

#endif //__MSXDEVICE_H__

