// $Id$

#ifndef __MSXZ80_HH__
#define __MSXZ80_HH__

#include <iostream.h>
#include "MSXDevice.hh"
#include "emutime.hh"

#include "Z80.hh"


class MSXZ80 : public MSXDevice
{
friend int Z80_SingleInstruction(void);
friend void Z80_Out (byte Port,byte Value);
friend byte Z80_RDMEM(word A);
friend byte Z80_In (byte Port);
friend void Z80_WRMEM(word A,byte V);

  public:
		static MSXDevice* Motherb; //used to delegate read/write-IO/Mem methods
		//destructor and 
		MSXZ80(void);
		~MSXZ80(void);
		//
		void init(void);
		//
		// old remarks please ignore
		// //instantiate method used in DeviceFactory
		//Must be overwritten in derived classes !!!!
		//but this is a slave device of MSXCPU
		// static MSXDevice* instantiate(void);
		
		// interaction with CPU
		//int executeTStates(int TStates);
		
		// executes the current command
		// returns the number of used T-states
		// int executeTStates(void);
		void setTargetTStates(Emutime &time);
		void executeUntilEmuTime(Emutime &time);


        private:
		Emutime currentCPUTime;
		Emutime targetCPUTime;
};
#endif //__MSXZ80_H__
