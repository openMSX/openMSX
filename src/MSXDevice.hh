// $Id$

#ifndef __MSXDEVICE_HH__
#define __MSXDEVICE_HH__

#include <iostream>
#include <fstream>
#include <string>
#include "msxconfig.hh"
#include "emutime.hh"
#include "openmsx.hh"
#include "Scheduler.hh"

class MSXDevice : public Schedulable
{
	public:
		virtual ~MSXDevice();
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
		
		virtual void saveState(std::ofstream &writestream);
		virtual void restoreState(std::string &devicestring, std::ifstream &readstream);
		
		virtual const std::string &getName();

	protected:
		MSXDevice();
		
		MSXConfig::Device *deviceConfig;
		//These are used for save/restoreState see note over
		//savefile-structure
		bool writeSaveStateHeader(std::ofstream &writestream);
		bool checkSaveStateHeader(std::string &devicestring);
		const std::string* deviceName;
		char* deviceVersion;
		
		// To ease the burden of keeping IRQ state
		void setInterrupt();
		void resetInterrupt();
		bool isIRQset;
};

#endif //__MSXDEVICE_HH__

