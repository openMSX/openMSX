// $Id$

#ifndef __MSXZ80_H__
#define __MSXZ80_H__

#include <iostream.h>
#include "MSXDevice.hh"

#include "Z80.hh"


class MSXZ80 : public MSXDevice
{
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
		void setTargetTStates(UINT64 TStates);
		void executeUntilEmuTime(UINT64 TStates);


        private:
		UINT64 CurrentCPUTime;
		UINT64 TargetCPUTime;
		int timeScaler; // factor between EmuTime and native T-states
};
#endif //__MSXZ80_H__
